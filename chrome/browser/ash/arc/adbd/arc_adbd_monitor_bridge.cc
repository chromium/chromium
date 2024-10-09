// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/adbd/arc_adbd_monitor_bridge.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/system/statistics_provider.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// The "_2d" in job names below corresponds to "-". Upstart escapes characters
// that aren't valid in D-Bus object paths with underscore followed by its
// ascii code in hex. So "arc_2dcreate_2ddata" becomes "arc-create-data".
constexpr const char kArcVmAdbdJobName[] = "arcvm_2dadbd";
// developer mode
constexpr const char kCrosDebug[] = "cros_debug";
// udc enabled
constexpr const char kCrosUdcEnabled[] = "dev_enable_udc";
// adbd.json
constexpr const char kAdbdJson[] = "/etc/arc/adbd.json";

bool g_enable_adb_over_usb_for_testing = false;

// Singleton factory for ArcAdbdMonitorBridge.
class ArcAdbdMonitorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAdbdMonitorBridge,
          ArcAdbdMonitorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAdbdMonitorBridgeFactory";

  static ArcAdbdMonitorBridgeFactory* GetInstance() {
    return base::Singleton<ArcAdbdMonitorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAdbdMonitorBridgeFactory>;
  ArcAdbdMonitorBridgeFactory() = default;
  ~ArcAdbdMonitorBridgeFactory() override = default;
};

// Returns true if the daemon for adb-over-usb should be started on the device.
bool ShouldStartAdbd(bool is_dev_mode,
                     bool is_host_on_vm,
                     bool has_adbd_json,
                     bool is_adb_over_usb_disabled) {
  // Do the same check as ArcSetup::MaybeStartAdbdProxy().
  return is_dev_mode && !is_host_on_vm && has_adbd_json &&
         !is_adb_over_usb_disabled;
}

// Returns true if adb-over-usb feature is enabled on Chrome OS.
bool IsAdbOverUsbEnabled() {
  // True when in developer mode.
  bool is_dev_mode = GetSystemPropertyInt(kCrosDebug) == 1;
  // True when udc is disabled.
  bool udc_disabled = GetSystemPropertyInt(kCrosUdcEnabled) == 0;
  // True when adbd json is established. Required for adb-over-usb feature.
  bool has_adbd_json = base::PathExists(base::FilePath(kAdbdJson));
  // True when the *host* is running on a VM.
  bool is_host_on_vm =
      ash::system::StatisticsProvider::GetInstance()->IsRunningOnVm();
  bool is_adb_over_usb_enabled =
      ShouldStartAdbd(is_dev_mode, is_host_on_vm, has_adbd_json, udc_disabled);
  return g_enable_adb_over_usb_for_testing || is_adb_over_usb_enabled;
}

// Returns cid from vm info. Otherwise, return nullopt.
std::optional<int64_t> GetCid() {
  Profile* const profile = arc::ArcSessionManager::Get()->profile();
  if (!profile) {
    LOG(ERROR) << "Profile is not ready";
    return std::nullopt;
  }
  const auto& vm_info =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)->GetVmInfo(
          kArcVmName);
  if (!vm_info) {
    LOG(ERROR) << "ARCVM is NOT ready";
    return std::nullopt;
  }
  return vm_info->cid();
}

std::string GetSerialNumber() {
  return arc::ArcSessionManager::Get()->GetSerialNumber();
}

std::optional<std::vector<std::string>> CreateAndGetAdbdUpstartEnvironment() {
  auto cid = GetCid();
  if (!cid) {
    LOG(ERROR) << "ARCVM cid is empty";
    return std::nullopt;
  }
  auto serial_number = GetSerialNumber();
  if (serial_number.empty()) {
    LOG(ERROR) << "Serial number is empty";
    return std::nullopt;
  }

  std::vector<std::string> environment = {
      "SERIALNUMBER=" + serial_number,
      base::StringPrintf("ARCVM_CID=%" PRId64, cid.value())};
  return environment;
}

}  // namespace

// static
ArcAdbdMonitorBridge* ArcAdbdMonitorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAdbdMonitorBridgeFactory::GetForBrowserContext(context);
}

// static
ArcAdbdMonitorBridge* ArcAdbdMonitorBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcAdbdMonitorBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcAdbdMonitorBridge::ArcAdbdMonitorBridge(content::BrowserContext* context,
                                           ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  VLOG(1) << "Init ArcAdbdMonitorBridge";
  arc_bridge_service_->adbd_monitor()->SetHost(this);
  arc_bridge_service_->adbd_monitor()->AddObserver(this);
}

ArcAdbdMonitorBridge::~ArcAdbdMonitorBridge() {
  arc_bridge_service_->adbd_monitor()->RemoveObserver(this);
  arc_bridge_service_->adbd_monitor()->SetHost(nullptr);
}

void ArcAdbdMonitorBridge::AdbdStarted() {
  VLOG(1) << "Received adbd start signal arcvm-adbd";
  StartArcVmAdbd(base::DoNothing());
}

void ArcAdbdMonitorBridge::AdbdStopped() {
  StopArcVmAdbd(base::DoNothing());
}

void ArcAdbdMonitorBridge::OnConnectionReady() {
  VLOG(1) << "Mojo connection is ready";
}

void ArcAdbdMonitorBridge::EnableAdbOverUsbForTesting() {
  g_enable_adb_over_usb_for_testing = true;
}

void ArcAdbdMonitorBridge::OnAdbdStartedForTesting(
    chromeos::VoidDBusMethodCallback callback) {
  StartArcVmAdbd(std::move(callback));
}

void ArcAdbdMonitorBridge::OnAdbdStoppedForTesting(
    chromeos::VoidDBusMethodCallback callback) {
  StopArcVmAdbd(std::move(callback));
}

void ArcAdbdMonitorBridge::StartArcVmAdbd(
    chromeos::VoidDBusMethodCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&IsAdbOverUsbEnabled),
      base::BindOnce(&ArcAdbdMonitorBridge::StartArcVmAdbdInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcAdbdMonitorBridge::StartArcVmAdbdInternal(
    chromeos::VoidDBusMethodCallback callback,
    bool adb_over_usb_enabled) {
  if (!adb_over_usb_enabled) {
    // No need to start arcvm-adbd job. Run the |callback| now.
    std::move(callback).Run(/*result=*/true);
    return;
  }
  // Start the daemon for supporting adb-over-usb.
  VLOG(1) << "Starting arcvm-adbd";
  auto environment = CreateAndGetAdbdUpstartEnvironment();
  if (!environment) {
    LOG(ERROR)
        << "Cannot start arcvm-adbd job since upstart environment is unknown";
    std::move(callback).Run(/*result=*/false);
    return;
  }

  std::deque<JobDesc> jobs{JobDesc{kArcVmAdbdJobName,
                                   UpstartOperation::JOB_STOP_AND_START,
                                   std::move(environment.value())}};
  ConfigureUpstartJobs(std::move(jobs), std::move(callback));
}

void ArcAdbdMonitorBridge::StopArcVmAdbd(
    chromeos::VoidDBusMethodCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&IsAdbOverUsbEnabled),
      base::BindOnce(&ArcAdbdMonitorBridge::StopArcVmAdbdInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcAdbdMonitorBridge::StopArcVmAdbdInternal(
    chromeos::VoidDBusMethodCallback callback,
    bool adb_over_usb_enabled) {
  if (!adb_over_usb_enabled) {
    // No need to stop arcvm-adbd job. Run the |callback| now.
    std::move(callback).Run(/*result=*/true);
    return;
  }
  // Stop the daemon for supporting adb-over-usb.
  VLOG(1) << "Stopping arcvm-adbd";
  auto environment = CreateAndGetAdbdUpstartEnvironment();
  if (!environment) {
    // Adbd upstart environment is not ready, please see error log.
    LOG(ERROR) << "Cannot stop arcvm-adbd job since upstart environment "
                  "is unknown";
    std::move(callback).Run(/*result=*/false);
    return;
  }

  std::deque<JobDesc> jobs{JobDesc{kArcVmAdbdJobName,
                                   UpstartOperation::JOB_STOP,
                                   std::move(environment.value())}};
  ConfigureUpstartJobs(std::move(jobs), std::move(callback));
}

// static
void ArcAdbdMonitorBridge::EnsureFactoryBuilt() {
  ArcAdbdMonitorBridgeFactory::GetInstance();
}

}  // namespace arc
