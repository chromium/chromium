// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_audio_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_pip_window_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_provisioning_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_switch_throttle_observer.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace arc {

namespace {

enum class UnthrottlingReason {
  // Unthrottling ARC to make its boot faster.
  kFasterBoot = 0,
  // Unthrottling ARC to prevent ANRs from happening.
  kPreAnr = 1,
  // All others.
  kOther = 2,
};

enum class CpuRestrictionVmResult {
  // Successfully set/reset CPU restrictions in ARCVM.
  kSuccess = 0,
  // Other failure reason.
  kOther = 1,
  // VM concierge service is not available.
  kNoConciergeService = 2,
  // VM concierge client is not available.
  kNoConciergeClient = 3,
  // VM Concierge did not respond.
  kConciergeDidNotRespond = 4,

  // Note: kMaxValue is needed only for histograms.
  kMaxValue = kConciergeDidNotRespond,
};

void RecordCpuRestrictionVMResult(CpuRestrictionVmResult result) {
  base::UmaHistogramEnumeration("Arc.CpuRestrictionVmResult", result);
}

// Checks all the |observers| for active ones to find out the reason why the
// instance is being unthrottled.
// This function can only be called when the instance is being unthrottled.
UnthrottlingReason GetUnthrottlingReason(
    const std::vector<std::unique_ptr<ash::ThrottleObserver>>& observers) {
  std::vector<ash::ThrottleObserver*> active_observers;

  // Check which observer(s) are active.
  for (const auto& observer : observers) {
    if (observer->active())
      active_observers.push_back(observer.get());
  }

  UnthrottlingReason result = UnthrottlingReason::kOther;
  DCHECK(!active_observers.empty()) << "All observers are inactive";

  for (const auto* active_observer : active_observers) {
    if (active_observer->name() == kArcBootPhaseThrottleObserverName) {
      result = UnthrottlingReason::kFasterBoot;
    } else if (active_observer->name() == kArcPowerThrottleObserverName) {
      result = UnthrottlingReason::kPreAnr;
    } else {
      // If an unknown observer is found, return kOther immediately.
      return UnthrottlingReason::kOther;
    }
  }

  // Note: If both ArcBootPhaseThrottleObserver and ArcPowerThrottleObserver are
  // active, either kFasterBoot or kPreAnr is returned as a special case.
  return result;
}

void OnSetArcVmCpuRestriction(
    std::optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call SetVmCpuRestriction";
    RecordCpuRestrictionVMResult(
        CpuRestrictionVmResult::kConciergeDidNotRespond);
    return;
  }
  if (response->success()) {
    RecordCpuRestrictionVMResult(CpuRestrictionVmResult::kSuccess);
  } else {
    LOG(ERROR) << "SetVmCpuRestriction for ARCVM failed";
    RecordCpuRestrictionVMResult(CpuRestrictionVmResult::kOther);
  }
}

void SetArcVmCpuRestrictionImpl(
    vm_tools::concierge::SetVmCpuRestrictionRequest request,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR)
        << "vm_concierge is not available. ArcInstanceThrottle won't work.";
    RecordCpuRestrictionVMResult(CpuRestrictionVmResult::kNoConciergeService);
    return;
  }

  auto* const client = ash::ConciergeClient::Get();
  // TODO(khmel): This should never be possible. Confirm via histogram and
  // change to DCHECK.
  if (!client) {
    LOG(ERROR) << "ConciergeClient is not available";
    RecordCpuRestrictionVMResult(CpuRestrictionVmResult::kNoConciergeClient);
    return;
  }

  client->SetVmCpuRestriction(request,
                              base::BindOnce(&OnSetArcVmCpuRestriction));
}

void SetArcVmCpuRestriction(CpuRestrictionState cpu_restriction_state,
                            bool use_quota) {
  auto* const client = ash::ConciergeClient::Get();
  // TODO(khmel): This should never be possible. Confirm via histogram and
  // change to DCHECK.
  if (!client) {
    LOG(ERROR) << "ConciergeClient is not available";
    RecordCpuRestrictionVMResult(CpuRestrictionVmResult::kNoConciergeClient);
    return;
  }

  vm_tools::concierge::SetVmCpuRestrictionRequest request;
  request.set_cpu_cgroup(vm_tools::concierge::CPU_CGROUP_ARCVM);
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      DCHECK(!use_quota);
      request.set_cpu_restriction_state(
          vm_tools::concierge::CPU_RESTRICTION_FOREGROUND);
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      request.set_cpu_restriction_state(
          use_quota ? vm_tools::concierge::
                          CPU_RESTRICTION_BACKGROUND_WITH_CFS_QUOTA_ENFORCED
                    : vm_tools::concierge::CPU_RESTRICTION_BACKGROUND);
      break;
  }

  // Unlike the ARC container code where the counterpart (session_manager) is
  // always available, this ARCVM function might be called before vm_concierge
  // is ready. To handle that case, send the D-Bus message via the callback
  // passed to the WaitForServiceToBeAvailable function.
  client->WaitForServiceToBeAvailable(
      base::BindOnce(&SetArcVmCpuRestrictionImpl, std::move(request)));
}

void SetArcCpuRestrictionCallback(
    login_manager::ContainerCpuRestrictionState state,
    bool success) {
  if (success)
    return;
  const char* message =
      (state == login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND)
          ? "unprioritize"
          : "prioritize";
  LOG(ERROR) << "Failed to " << message << " ARC";
}

void SetArcContainerCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  if (!ash::SessionManagerClient::Get()) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }

  login_manager::ContainerCpuRestrictionState state;
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_FOREGROUND;
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND;
      break;
  }
  ash::SessionManagerClient::Get()->SetArcCpuRestriction(
      state, base::BindOnce(SetArcCpuRestrictionCallback, state));
}

// Adjusts the amount of CPU the ARC instance is allowed to use. When
// |cpu_restriction_state| is CPU_RESTRICTION_BACKGROUND, the limit is adjusted
// so ARC can only use tightly restricted CPU resources.
void SetArcCpuRestriction(CpuRestrictionState cpu_restriction_state,
                          bool use_quota) {
  if (IsArcVmEnabled()) {
    SetArcVmCpuRestriction(cpu_restriction_state, use_quota);
  } else {
    // ARC container does not support |use_quota|.
    SetArcContainerCpuRestriction(cpu_restriction_state);
  }
}

class DefaultDelegateImpl : public ArcInstanceThrottle::Delegate {
 public:
  DefaultDelegateImpl() = default;

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;
  void SetCpuRestriction(CpuRestrictionState cpu_restriction_state,
                         bool use_quota) override {
    SetArcCpuRestriction(cpu_restriction_state, use_quota);
  }

  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {
    DVLOG(2) << "ARC throttling was disabled for "
             << delta.InMillisecondsRoundedUp()
             << " ms due to: " << observer_name;
    UmaHistogramLongTimes("Arc.CpuRestrictionDisabled." + observer_name, delta);
  }
};

// Singleton factory for ArcInstanceThrottle.
class ArcInstanceThrottleFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInstanceThrottle,
          ArcInstanceThrottleFactory> {
 public:
  static constexpr const char* kName = "ArcInstanceThrottleFactory";
  static ArcInstanceThrottleFactory* GetInstance() {
    static base::NoDestructor<ArcInstanceThrottleFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<ArcInstanceThrottleFactory>;

  ArcInstanceThrottleFactory() {
    DependsOn(ArcBootPhaseMonitorBridgeFactory::GetInstance());
    DependsOn(ArcMetricsServiceFactory::GetInstance());
    DependsOn(ArcAppLaunchNotifierFactory::GetInstance());
  }
  ~ArcInstanceThrottleFactory() override = default;
};

CpuRestrictionState ToCpuRestriction(bool should_throttle) {
  return should_throttle ? CpuRestrictionState::CPU_RESTRICTION_BACKGROUND
                         : CpuRestrictionState::CPU_RESTRICTION_FOREGROUND;
}

}  // namespace

// static
const char ArcInstanceThrottle::kChromeArcPowerControlPageObserver[] =
    "arc-power-control";

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContext(context);
}

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContextForTesting(context);
}

// Note: This function (especially the AddObserver and StartObservers part) must
// be called right after Chrome browser starts, without waiting for vm_concierge
// to be ready. Otherwise, some of the observers might miss events and fail to
// throttle the instance correctly.
ArcInstanceThrottle::ArcInstanceThrottle(content::BrowserContext* context,
                                         ArcBridgeService* bridge)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()),
      bridge_(bridge) {
  // Note: When you add a new observer, consider modifying GetUnthrottlingReason
  // so that the CPU quota enforcement feature continues to work as intended. In
  // general, unthrottling that is done automatically without the user's action
  // might have to be listed in the UnthrottlingReason enum class. See the
  // comment in ArcInstanceThrottle::ThrottleInstance too.

  AddObserver(std::make_unique<ArcActiveWindowThrottleObserver>());
  AddObserver(std::make_unique<ArcAppLaunchThrottleObserver>());
  AddObserver(std::make_unique<ArcBootPhaseThrottleObserver>());
  AddObserver(std::make_unique<ArcPipWindowThrottleObserver>());
  AddObserver(std::make_unique<ArcPowerThrottleObserver>());
  AddObserver(std::make_unique<ArcProvisioningThrottleObserver>());
  AddObserver(std::make_unique<ArcSwitchThrottleObserver>());
  // This one is controlled by ash::ArcPowerControlHandler.
  AddObserver(std::make_unique<ash::ThrottleObserver>(
      kChromeArcPowerControlPageObserver));
  if (base::FeatureList::IsEnabled(arc::kUnthrottleOnActiveAudioV2)) {
    AddObserver(std::make_unique<ArcActiveAudioThrottleObserver>());
  }

  StartObservers();
  DCHECK(bridge_);
  bridge_->power()->AddObserver(this);

  ArcMetricsService::GetForBrowserContext(context)->AddBootTypeObserver(this);
}

ArcInstanceThrottle::~ArcInstanceThrottle() = default;

void ArcInstanceThrottle::Shutdown() {
  ArcMetricsService::GetForBrowserContext(context())->RemoveBootTypeObserver(
      this);

  bridge_->power()->RemoveObserver(this);

  StopObservers();
}

void ArcInstanceThrottle::OnConnectionReady() {
  NotifyCpuRestriction(ToCpuRestriction(should_throttle()));
}

void ArcInstanceThrottle::OnBootTypeRetrieved(mojom::BootType boot_type) {
  switch (boot_type) {
    case mojom::BootType::UNKNOWN:
      break;
    case mojom::BootType::FIRST_BOOT:
    case mojom::BootType::FIRST_BOOT_AFTER_UPDATE:
      // ARCVM vCPUs tend to be very busy on those boots. Allow Chrome to use
      // the enforcing quota mode to cap the VM's CPU usage.
      return;
    case mojom::BootType::REGULAR_BOOT:
      // On the other hand, regular boot does not usually consume that much vCPU
      // time. Disable quota enforcement now to prevent unnecessary ANRs from
      // happening.
      never_enforce_quota_ = true;
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void ArcInstanceThrottle::ThrottleInstance(bool should_throttle) {
  const CpuRestrictionState cpu_restriction_state =
      ToCpuRestriction(should_throttle);

  NotifyCpuRestriction(cpu_restriction_state);

  // Check if enforcing quota is possible
  bool use_quota = false;

  if (!should_throttle && !never_enforce_quota_) {
    // We're unthrottling the instance. Figure out why.
    switch (GetUnthrottlingReason(observers())) {
      case UnthrottlingReason::kFasterBoot:
        DVLOG(2) << "Unthrottling for faster boot. Quota is still applicable.";
        break;
      case UnthrottlingReason::kPreAnr:
        DVLOG(2)
            << "Unthrottling for preventing ANRs. Quota is still applicable.";
        break;
      case UnthrottlingReason::kOther:
        // ARC is unthrottled by a user action. After such an event, the quota
        // should never be applied.
        DVLOG(2) << "Unthrottling because of a user action. Quota is no longer "
                    "applicable.";
        never_enforce_quota_ = true;
        break;
    }
    // Note on why other throttle observers that don't monitor the user's action
    // can be classified as |kOther| and can disable quota:
    //
    // * ArcSwitchThrottleObserver:
    //   This is for disabling throttling for testing. If the observer gets
    //   activated, the quota shouldn't be applied either.
    //
    // * ArcProvisioningThrottleObserver:
    //   If this gets activated, the provisioning is ongoing. The quota
    //   shouldn't be applied to make provisioning failures less likely to
    //   happen.
  }

  const std::optional<bool>& arc_is_booting =
      GetBootObserver()->arc_is_booting();
  const bool arc_has_booted = (arc_is_booting && !*arc_is_booting);
  const bool is_throttling = (cpu_restriction_state ==
                              CpuRestrictionState::CPU_RESTRICTION_BACKGROUND);

  if (arc_has_booted && !never_enforce_quota_ && is_throttling) {
    // TODO(khmel): Do not use quota when Android VPN is in use.
    use_quota = true;
    DVLOG(2) << "Enforcing cfs_quota";
  }
  delegate_->SetCpuRestriction(cpu_restriction_state, use_quota);
}

void ArcInstanceThrottle::RecordCpuRestrictionDisabledUMA(
    const std::string& observer_name,
    base::TimeDelta delta) {
  delegate_->RecordCpuRestrictionDisabledUMA(observer_name, delta);
}

void ArcInstanceThrottle::NotifyCpuRestriction(
    CpuRestrictionState cpu_restriction_state) {
  auto* power =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_->power(), OnCpuRestrictionChanged);
  if (!power)
    return;
  power->OnCpuRestrictionChanged(
      static_cast<mojom::CpuRestrictionState>(cpu_restriction_state));
}

ArcBootPhaseThrottleObserver* ArcInstanceThrottle::GetBootObserver() {
  ash::ThrottleObserver* observer =
      GetObserverByName(kArcBootPhaseThrottleObserverName);
  return static_cast<ArcBootPhaseThrottleObserver*>(observer);
}

// static
void ArcInstanceThrottle::EnsureFactoryBuilt() {
  ArcInstanceThrottleFactory::GetInstance();
}

}  // namespace arc
