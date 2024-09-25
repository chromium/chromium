// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/arc/metrics/arc_metrics_service.h"

#include <sys/sysinfo.h>

#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_anr.h"
#include "ash/components/arc/metrics/arc_wm_metrics.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/exo/wm_helper.h"
#include "components/metrics/psi_memory_parser.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace arc {

namespace {

constexpr char kUmaPrefix[] = "Arc";

constexpr base::TimeDelta kUmaMinTime = base::Milliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::Seconds(60);
constexpr int kUmaNumBuckets = 50;
constexpr int kUmaPriAbiMigMaxFailedAttempts = 10;
constexpr int kUmaDataSizeInMBMin = 1;
constexpr int kUmaDataSizeInMBMax = 1000000;  // 1 TB.

constexpr base::TimeDelta kRequestProcessListPeriod = base::Minutes(5);
constexpr char kArcProcessNamePrefix[] = "org.chromium.arc.";
constexpr char kGmsProcessNamePrefix[] = "com.google.android.gms";
constexpr char kBootProgressEnableScreen[] = "boot_progress_enable_screen";
constexpr char kBootProgressArcUpgraded[] = "boot_progress_arc_upgraded";

// Interval for collecting UMA "Arc.App.LowMemoryKills.*Count10Minutes" metrics.
constexpr base::TimeDelta kRequestKillCountPeriod = base::Minutes(10);

// Memory pressure histograms.
const char kPSIMemoryPressureSomeARC[] = "ChromeOS.CWP.PSIMemPressure.ArcSome";
const char kPSIMemoryPressureFullARC[] = "ChromeOS.CWP.PSIMemPressure.ArcFull";

// Provisioning pre-sign-in time delta histogram.
constexpr char kProvisioningPreSignInTimeDelta[] =
    "Arc.Provisioning.PreSignIn.TimeDelta";

// Logs UMA enum values to facilitate finding feedback reports in Xamine.
template <typename T>
void LogStabilityUmaEnum(const std::string& name, T sample) {
  base::UmaHistogramEnumeration(name, sample);
  VLOG(1) << name << ": " << static_cast<std::underlying_type_t<T>>(sample);
}

std::string BootTypeToString(mojom::BootType boot_type) {
  switch (boot_type) {
    case mojom::BootType::UNKNOWN:
      break;
    case mojom::BootType::FIRST_BOOT:
      return ".FirstBoot";
    case mojom::BootType::FIRST_BOOT_AFTER_UPDATE:
      return ".FirstBootAfterUpdate";
    case mojom::BootType::REGULAR_BOOT:
      return ".RegularBoot";
  }
  DUMP_WILL_BE_NOTREACHED();
  return "";
}

const char* DnsQueryToString(mojom::ArcDnsQuery query) {
  switch (query) {
    case mojom::ArcDnsQuery::OTHER_HOST_NAME:
      return "Other";
    case mojom::ArcDnsQuery::ANDROID_API_HOST_NAME:
      return "AndroidApi";
  }
  NOTREACHED();
}

const char* WaylandTimingEventToString(mojom::WaylandTimingEvent event) {
  switch (event) {
    case mojom::WaylandTimingEvent::kOther:
      return ".Other";
    case mojom::WaylandTimingEvent::kBinderReleaseClipboardData:
      return ".BinderReleaseClipboardData";
    case mojom::WaylandTimingEvent::kWlBufferRelease:
      return ".WlBufferRelease";
    case mojom::WaylandTimingEvent::kWlKeyboardLeave:
      return ".WlKeyboardLeave";
    case mojom::WaylandTimingEvent::kWlPointerMotion:
      return ".WlPointerMotion";
    case mojom::WaylandTimingEvent::kWlPointerLeave:
      return ".WlPointerLeave";
    case mojom::WaylandTimingEvent::kZauraShellActivated:
      return ".ZauraShellActivated";
    case mojom::WaylandTimingEvent::kZauraSurfaceOcclusionChanged:
      return ".ZauraSurfaceOcclusionChanged";
    case mojom::WaylandTimingEvent::kZcrRemoteSurfaceWindowGeometryChanged:
      return ".ZcrRemoteSurfaceWindowGeometryChanged";
    case mojom::WaylandTimingEvent::kZcrRemoteSurfaceBoundsChangedInOutput:
      return ".ZcrRemoteSurfaceBoundsChangedInOutput";
    case mojom::WaylandTimingEvent::kZcrVsyncTimingUpdate:
      return ".ZcrVsyncTimingUpdate";
  }
  NOTREACHED();
}

// Converts mojom::AndroidAppCategory to AndroidAppCategories in
// tools/metrics/histograms/metadata/arc/histograms.xml
const char* AndroidAppCategoryToString(mojom::AndroidAppCategory input) {
  switch (input) {
    case mojom::AndroidAppCategory::kOther:
      return "Other";
    case mojom::AndroidAppCategory::kAudio:
      return "Audio";
    case mojom::AndroidAppCategory::kGame:
      return "Game";
    case mojom::AndroidAppCategory::kImage:
      return "Image";
    case mojom::AndroidAppCategory::kProductivity:
      return "Productivity";
    case mojom::AndroidAppCategory::kSocial:
      return "Social";
    case mojom::AndroidAppCategory::kVideo:
      return "Video";
  }
  NOTREACHED();
}

// Converts mojom::AndroidDataDirectory to AndroidDataDirectories in
// tools/metrics/histograms/metadata/arc/histograms.xml
const char* AndroidDataDirectoryToString(mojom::AndroidDataDirectory input) {
  switch (input) {
    case mojom::AndroidDataDirectory::kData:
      return "Data";
    case mojom::AndroidDataDirectory::kDataApp:
      return "DataApp";
    case mojom::AndroidDataDirectory::kDataData:
      return "DataData";
    case mojom::AndroidDataDirectory::kDataMedia:
      return "DataMedia";
    case mojom::AndroidDataDirectory::kDataMediaAndroid:
      return "DataMediaAndroid";
    case mojom::AndroidDataDirectory::kDataUserDE:
      return "DataUserDE";
  }
  NOTREACHED();
}

struct LoadAverageHistogram {
  const char* name;
  base::TimeDelta duration;
};
constexpr LoadAverageHistogram kLoadAverageHistograms[] = {
    {"Arc.LoadAverageX100PerProcessor1MinuteAfterArcStart", base::Minutes(1)},
    {"Arc.LoadAverageX100PerProcessor5MinutesAfterArcStart", base::Minutes(5)},
    {"Arc.LoadAverageX100PerProcessor15MinutesAfterArcStart",
     base::Minutes(15)},
};

}  // namespace

// static
ArcMetricsServiceFactory* ArcMetricsServiceFactory::GetInstance() {
  return base::Singleton<ArcMetricsServiceFactory>::get();
}

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContext(context);
}

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContextForTesting(context);
}

// static
BrowserContextKeyedServiceFactory* ArcMetricsService::GetFactory() {
  return ArcMetricsServiceFactory::GetInstance();
}

ArcMetricsService::ArcMetricsService(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      guest_os_engagement_metrics_(user_prefs::UserPrefs::Get(context),
                                   base::BindRepeating(ash::IsArcWindow),
                                   prefs::kEngagementPrefsPrefix,
                                   kUmaPrefix),
      process_observer_(this),
      intent_helper_observer_(this, &arc_bridge_service_observer_),
      app_launcher_observer_(this, &arc_bridge_service_observer_) {
  arc_bridge_service_->AddObserver(&arc_bridge_service_observer_);
  arc_bridge_service_->app()->AddObserver(&app_launcher_observer_);
  arc_bridge_service_->intent_helper()->AddObserver(&intent_helper_observer_);
  arc_bridge_service_->metrics()->SetHost(this);
  arc_bridge_service_->process()->AddObserver(&process_observer_);
  // If WMHelper doesn't exist, do nothing. This occurs in tests.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  ui::GamepadProviderOzone::GetInstance()->AddGamepadObserver(this);

  DCHECK(StabilityMetricsManager::Get());
  StabilityMetricsManager::Get()->SetArcNativeBridgeType(
      NativeBridgeType::UNKNOWN);

  if (base::FeatureList::IsEnabled(kVmMemoryPSIReports)) {
    psi_parser_ = std::make_unique<metrics::PSIMemoryParser>(
        kVmMemoryPSIReportsPeriod.Get());
  }

  arc_wm_metrics_ = std::make_unique<ArcWmMetrics>();
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/40531599): Fix shutdown order.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
  arc_bridge_service_->process()->RemoveObserver(&process_observer_);
  arc_bridge_service_->metrics()->SetHost(nullptr);
  arc_bridge_service_->intent_helper()->RemoveObserver(
      &intent_helper_observer_);
  arc_bridge_service_->app()->RemoveObserver(&app_launcher_observer_);
  arc_bridge_service_->RemoveObserver(&arc_bridge_service_observer_);
}

void ArcMetricsService::Shutdown() {
  metrics_anr_.reset();
  for (auto& obs : app_kill_observers_)
    obs.OnArcMetricsServiceDestroyed();
  app_kill_observers_.Clear();
}

// static
void ArcMetricsService::RecordArcUserInteraction(
    content::BrowserContext* context,
    UserInteractionType type) {
  DCHECK(context);
  auto* service = GetForBrowserContext(context);
  if (!service) {
    LOG(WARNING) << "Cannot get ArcMetricsService for context " << context;
    return;
  }
  service->RecordArcUserInteraction(type);
}

void ArcMetricsService::RecordArcUserInteraction(UserInteractionType type) {
  UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction", type);
  for (auto& obs : user_interaction_observers_)
    obs.OnUserInteraction(type);
}

void ArcMetricsService::SetHistogramNamerCallback(
    HistogramNamerCallback histogram_namer_cb) {
  histogram_namer_cb_ = histogram_namer_cb;
}

void ArcMetricsService::OnProcessConnectionReady() {
  VLOG(2) << "Start updating process list.";
  request_process_list_timer_.Start(FROM_HERE, kRequestProcessListPeriod, this,
                                    &ArcMetricsService::RequestProcessList);

  if (IsArcVmEnabled()) {
    prev_logged_memory_kills_.reset();
    // Initialize prev_logged_memory_kills_ by immediately requesting new
    // values. We don't need the VM list to exist to update it, so pass nullopt.
    OnListVmsResponse(std::nullopt);
    request_kill_count_timer_.Start(
        FROM_HERE, kRequestKillCountPeriod, this,
        &ArcMetricsService::OnRequestKillCountTimer);
  }
}

void ArcMetricsService::OnProcessConnectionClosed() {
  VLOG(2) << "Stop updating process list.";
  request_process_list_timer_.Stop();
  request_kill_count_timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestProcessList);
  if (!process_instance)
    return;
  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(base::BindOnce(
      &ArcMetricsService::ParseProcessList, weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const std::string& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name, kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name, kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

// Helper that logs a single kill count type to a histogram, or logs a warning
// if the counter decreased.
static void LogLowMemoryKillCount(const char* vm_desc,
                                  const char* name,
                                  uint32_t prev_kills,
                                  uint32_t curr_kills) {
  if (prev_kills <= curr_kills) {
    std::string histogram =
        base::StringPrintf("Arc.App.LowMemoryKills%s%s", vm_desc, name);
    base::UmaHistogramExactLinear(histogram, curr_kills - prev_kills, 50);
  } else {
    LOG(WARNING) << "LowMemoryKillCounts reported a decrease for " << vm_desc
                 << name << ", previous: " << prev_kills
                 << ", current: " << curr_kills;
  }
}

// Helper that logs kill counts for a specific background VM.
static void LogLowMemoryKillCountsForVm(
    const char* vm_desc,
    const mojom::LowMemoryKillCountsPtr& prev,
    const mojom::LowMemoryKillCountsPtr& curr) {
  LogLowMemoryKillCount(vm_desc, ".LinuxOOMCount10Minutes", prev->guest_oom,
                        curr->guest_oom);
  LogLowMemoryKillCount(vm_desc, ".LMKD.ForegroundCount10Minutes",
                        prev->lmkd_foreground, curr->lmkd_foreground);
  LogLowMemoryKillCount(vm_desc, ".LMKD.PerceptibleCount10Minutes",
                        prev->lmkd_perceptible, curr->lmkd_perceptible);
  LogLowMemoryKillCount(vm_desc, ".LMKD.CachedCount10Minutes",
                        prev->lmkd_cached, curr->lmkd_cached);
  LogLowMemoryKillCount(vm_desc, ".Pressure.ForegroundCount10Minutes",
                        prev->pressure_foreground, curr->pressure_foreground);
  LogLowMemoryKillCount(vm_desc, ".Pressure.PerceptibleCount10Minutes",
                        prev->pressure_perceptible, curr->pressure_perceptible);
  LogLowMemoryKillCount(vm_desc, ".Pressure.CachedCount10Minutes",
                        prev->pressure_cached, curr->pressure_cached);
}

static void LogLowMemoryKillCounts(
    const mojom::LowMemoryKillCountsPtr& prev,
    const mojom::LowMemoryKillCountsPtr& curr,
    std::optional<vm_tools::concierge::ListVmsResponse> vms_list) {
  // Only log to the histograms if we have a previous sample to compute deltas
  // from.
  if (!prev)
    return;

  LogLowMemoryKillCountsForVm("", prev, curr);

  // Only log the background VMs counters if we have a valid list of background
  // VMs.
  if (!vms_list || !vms_list->success())
    return;

  // Use a set to de-duplicate VM types.
  std::unordered_set<vm_tools::concierge::VmInfo_VmType> vm_types;
  for (int i = 0; i < vms_list->vms_size(); i++) {
    const auto& vm = vms_list->vms(i);
    if (vm.has_vm_info()) {
      const auto& info = vm.vm_info();
      vm_types.emplace(info.vm_type());
    } else {
      LOG(WARNING) << "OnLowMemoryKillCounts got VM " << vm.name()
                   << " with no vm_info.";
    }
  }
  for (auto vm_type : vm_types) {
    switch (vm_type) {
      case vm_tools::concierge::VmInfo_VmType_ARC_VM:
        if (vm_types.size() == 1) {
          // Only ARCVM, log to a special set of counters.
          LogLowMemoryKillCountsForVm(".OnlyArc", prev, curr);
        }
        break;

      case vm_tools::concierge::VmInfo_VmType_BOREALIS:
        LogLowMemoryKillCountsForVm(".Steam", prev, curr);
        break;

      case vm_tools::concierge::VmInfo_VmType_TERMINA:
        LogLowMemoryKillCountsForVm(".Crostini", prev, curr);
        break;

      case vm_tools::concierge::VmInfo_VmType_PLUGIN_VM:
        LogLowMemoryKillCountsForVm(".PluginVm", prev, curr);
        break;

      default:
        LogLowMemoryKillCountsForVm(".UnknownVm", prev, curr);
        break;
    }
  }
}

void ArcMetricsService::RequestKillCountsForTesting() {
  OnRequestKillCountTimer();
}

void ArcMetricsService::OnRequestKillCountTimer() {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(WARNING)
        << "Cannot get ConciergeClient for method OnRequestKillCountTimer";
    return;
  }
  vm_tools::concierge::ListVmsRequest request;
  request.set_owner_id(user_id_hash_);
  client->ListVms(request, base::BindOnce(&ArcMetricsService::OnListVmsResponse,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::OnListVmsResponse(
    std::optional<vm_tools::concierge::ListVmsResponse> response) {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestLowMemoryKillCounts);
  if (!process_instance) {
    LOG(WARNING) << "Cannot get ProcessInstance for method OnListVmsResponse";
    return;
  }
  process_instance->RequestLowMemoryKillCounts(
      base::BindOnce(&ArcMetricsService::OnLowMemoryKillCounts,
                     weak_ptr_factory_.GetWeakPtr(), response));
}

void ArcMetricsService::OnLowMemoryKillCounts(
    std::optional<vm_tools::concierge::ListVmsResponse> vms_list,
    mojom::LowMemoryKillCountsPtr counts) {
  DCHECK(daily_);
  if (daily_) {
    int oom = counts->guest_oom;
    int foreground = counts->lmkd_foreground + counts->pressure_foreground;
    int perceptible = counts->lmkd_perceptible + counts->pressure_foreground;
    int cached = counts->lmkd_cached + counts->pressure_cached;
    if (prev_logged_memory_kills_) {
      oom -= prev_logged_memory_kills_->guest_oom;
      foreground -= prev_logged_memory_kills_->lmkd_foreground +
                    prev_logged_memory_kills_->pressure_foreground;
      perceptible -= prev_logged_memory_kills_->lmkd_perceptible +
                     prev_logged_memory_kills_->pressure_perceptible;
      cached -= prev_logged_memory_kills_->lmkd_cached +
                prev_logged_memory_kills_->pressure_cached;
    }
    if (oom >= 0 && foreground >= 0 && perceptible >= 0 && cached >= 0) {
      daily_->OnLowMemoryKillCounts(vms_list, oom, foreground, perceptible,
                                    cached);
    } else {
      LOG(WARNING) << "OnLowMemoryKillCounts observed a decrease in monotonic "
                      "kill counters";
    }
  }

  LogLowMemoryKillCounts(prev_logged_memory_kills_, counts, vms_list);

  prev_logged_memory_kills_ = std::move(counts);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type,
    std::optional<base::TimeTicks> arc_start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!arc_start_time.has_value()) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  VLOG(2) << "ARC start @" << arc_start_time.value();

  DCHECK_NE(mojom::BootType::UNKNOWN, boot_type);
  const std::string suffix = BootTypeToString(boot_type);
  for (const auto& event : events) {
    VLOG(2) << "Report boot progress event:" << event->event << "@"
            << event->uptimeMillis;
    const base::TimeTicks uptime =
        base::Milliseconds(event->uptimeMillis) + base::TimeTicks();
    const base::TimeDelta elapsed_time = uptime - arc_start_time.value();
    if (event->event.compare(kBootProgressEnableScreen) == 0) {
      base::UmaHistogramCustomTimes("Arc.AndroidBootTime" + suffix,
                                    elapsed_time, kUmaMinTime, kUmaMaxTime,
                                    kUmaNumBuckets);
    }
  }
}

void ArcMetricsService::ReportBootProgress(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (boot_type == mojom::BootType::UNKNOWN) {
    LOG(WARNING) << "boot_type is unknown. Skip recording UMA.";
    return;
  }
  boot_type_ = boot_type;
  for (auto& obs : boot_type_observers_)
    obs.OnBootTypeRetrieved(boot_type);
  if (metrics_anr_)
    metrics_anr_->set_uma_suffix(BootTypeToString(boot_type));

  if (IsArcVmEnabled()) {
    // For VM builds, do not call into session_manager since we don't use it
    // for the builds. The upgrade time is included in the events vector so we
    // can extract it here.
    std::optional<base::TimeTicks> arc_start_time =
        GetArcStartTimeFromEvents(events);
    OnArcStartTimeRetrieved(std::move(events), boot_type, arc_start_time);
    return;
  }

  // Retrieve ARC full container's start time from session manager.
  ash::SessionManagerClient::Get()->GetArcStartTime(base::BindOnce(
      &ArcMetricsService::OnArcStartTimeRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(events), boot_type));

  // Record load average in case they are already measured.
  MaybeRecordLoadAveragePerProcessor();
}

void ArcMetricsService::ReportNativeBridge(
    mojom::NativeBridgeType mojo_native_bridge_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "Mojo native bridge type is " << mojo_native_bridge_type;

  NativeBridgeType native_bridge_type = NativeBridgeType::UNKNOWN;
  switch (mojo_native_bridge_type) {
    case mojom::NativeBridgeType::NONE:
      native_bridge_type = NativeBridgeType::NONE;
      break;
    case mojom::NativeBridgeType::HOUDINI:
      native_bridge_type = NativeBridgeType::HOUDINI;
      break;
    case mojom::NativeBridgeType::NDK_TRANSLATION:
      native_bridge_type = NativeBridgeType::NDK_TRANSLATION;
      break;
  }
  DCHECK_NE(native_bridge_type, NativeBridgeType::UNKNOWN)
      << mojo_native_bridge_type;

  StabilityMetricsManager::Get()->SetArcNativeBridgeType(native_bridge_type);
}

void ArcMetricsService::ReportCompanionLibApiUsage(
    mojom::CompanionLibApiId api_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UMA_HISTOGRAM_ENUMERATION("Arc.CompanionLibraryApisCounter", api_id);
}

void ArcMetricsService::ReportAppKill(mojom::AppKillPtr app_kill) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  switch (app_kill->type) {
    case mojom::AppKillType::LMKD_KILL:
      NotifyLowMemoryKill();
      break;
    case mojom::AppKillType::OOM_KILL:
      NotifyOOMKillCount(app_kill->count);
      break;
    case mojom::AppKillType::GMS_UPDATE_KILL:
    case mojom::AppKillType::GMS_START_KILL:
      for (uint32_t i = 0; i < app_kill->count; i++) {
        UMA_HISTOGRAM_ENUMERATION(
            "Arc.App.GmsCoreKill" + BootTypeToString(boot_type_),
            app_kill->type);
      }
      break;
  }
}

void ArcMetricsService::ReportDnsQueryResult(mojom::ArcDnsQuery query,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string metric_name =
      base::StrCat({"Arc.Net.DnsQuery.", DnsQueryToString(query)});
  if (!success)
    VLOG(4) << metric_name << ": " << success;
  base::UmaHistogramBoolean(metric_name, success);
}

void ArcMetricsService::NotifyLowMemoryKill() {
  for (auto& obs : app_kill_observers_)
    obs.OnArcLowMemoryKill();
}

void ArcMetricsService::NotifyOOMKillCount(unsigned long count) {
  for (auto& obs : app_kill_observers_)
    obs.OnArcOOMKillCount(count);
}

void ArcMetricsService::ReportAppPrimaryAbi(mojom::AppPrimaryAbi abi) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.App.PrimaryAbi", abi);
}

void ArcMetricsService::ReportArcCorePriAbiMigEvent(
    mojom::ArcCorePriAbiMigEvent event_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LogStabilityUmaEnum("Arc.AbiMigration.Event", event_type);
}

void ArcMetricsService::ReportArcCorePriAbiMigFailedTries(
    uint32_t failed_attempts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.AbiMigration.FailedAttempts", failed_attempts,
                             kUmaPriAbiMigMaxFailedAttempts);
}

void ArcMetricsService::ReportArcCorePriAbiMigDowngradeDelay(
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramCustomTimes("Arc.AbiMigration.DowngradeDelay", delay,
                                kUmaMinTime, kUmaMaxTime, kUmaNumBuckets);
}

void ArcMetricsService::OnArcStartTimeForPriAbiMigration(
    base::TimeTicks durationTicks,
    std::optional<base::TimeTicks> arc_start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!arc_start_time.has_value()) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  VLOG(2) << "ARC start for Primary Abi Migration @" << arc_start_time.value();

  const base::TimeDelta elapsed_time = durationTicks - arc_start_time.value();
  base::UmaHistogramCustomTimes("Arc.AbiMigration.BootTime", elapsed_time,
                                kUmaMinTime, kUmaMaxTime, kUmaNumBuckets);
}

void ArcMetricsService::ReportArcCorePriAbiMigBootTime(
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // For VM builds, we are directly reporting the boot time duration from
  // ARC Metrics code.
  if (IsArcVmEnabled()) {
    base::UmaHistogramCustomTimes("Arc.AbiMigration.BootTime", duration,
                                  kUmaMinTime, kUmaMaxTime, kUmaNumBuckets);
    return;
  }

  // For container builds, we report the time of boot_progress_enable_screen
  // event, and boot time duration is calculated by subtracting the ARC start
  // time, which is fetched from session manager.
  const base::TimeTicks durationTicks = duration + base::TimeTicks();
  // Retrieve ARC full container's start time from session manager.
  ash::SessionManagerClient::Get()->GetArcStartTime(
      base::BindOnce(&ArcMetricsService::OnArcStartTimeForPriAbiMigration,
                     weak_ptr_factory_.GetWeakPtr(), durationTicks));
}

void ArcMetricsService::ReportArcSystemHealthUpgrade(base::TimeDelta duration,
                                                     bool packages_deleted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramCustomTimes("Arc.SystemHealth.Upgrade.TimeDelta", duration,
                                kUmaMinTime, kUmaMaxTime, kUmaNumBuckets);

  base::UmaHistogramBoolean("Arc.SystemHealth.Upgrade.PackagesDeleted",
                            packages_deleted);
}

void ArcMetricsService::ReportAnr(mojom::AnrPtr anr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!metrics_anr_) {
    LOG(ERROR) << "ANR is reported when no ANR metric service is connected";
    return;
  }

  metrics_anr_->Report(std::move(anr));
}

void ArcMetricsService::ReportLowLatencyStylusLibApiUsage(
    mojom::LowLatencyStylusLibApiId api_id) {
  // Deprecated: This will be removed once all callers are removed.
}

void ArcMetricsService::ReportVpnServiceBuilderCompatApiUsage(
    mojom::VpnServiceBuilderCompatApiId api_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.VpnServiceBuilderCompatApisCounter",
                                api_id);
}

void ArcMetricsService::ReportLowLatencyStylusLibPredictionTarget(
    mojom::LowLatencyStylusLibPredictionTargetPtr prediction_target) {
  // Deprecated: This will be removed once all callers are removed.
}

void ArcMetricsService::ReportMainAccountHashMigrationMetrics(
    mojom::MainAccountHashMigrationStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UMA_HISTOGRAM_ENUMERATION("Arc.Auth.MainAccountHashMigration.Status", status);
}

void ArcMetricsService::ReportDataRestore(mojom::DataRestoreStatus status,
                                          int64_t duration_ms) {
  base::UmaHistogramEnumeration("Arc.DataRestore.Status", status);
  if (status == mojom::DataRestoreStatus::kNotNeeded)
    return;
  base::UmaHistogramMediumTimes("Arc.DataRestore.Duration",
                                base::Milliseconds(duration_ms));
}

void ArcMetricsService::ReportMemoryPressure(
    const std::vector<uint8_t>& psi_file_contents) {
  if (!psi_parser_) {
    LOG(WARNING) << "Unexpected PSI reporting call detected";
    return;
  }

  int metric_some;
  int metric_full;

  auto stat = psi_parser_->ParseMetrics(
      base::as_string_view(base::span(psi_file_contents)), &metric_some,
      &metric_full);
  psi_parser_->LogParseStatus(
      stat);  // Log success and failure, for histograms.
  if (stat != metrics::ParsePSIMemStatus::kSuccess)
    return;

  base::UmaHistogramCustomCounts(
      kPSIMemoryPressureSomeARC, metric_some, metrics::kMemPressureMin,
      metrics::kMemPressureExclusiveMax, metrics::kMemPressureHistogramBuckets);
  base::UmaHistogramCustomCounts(
      kPSIMemoryPressureFullARC, metric_full, metrics::kMemPressureMin,
      metrics::kMemPressureExclusiveMax, metrics::kMemPressureHistogramBuckets);
}

void ArcMetricsService::SetPrefService(PrefService* prefs) {
  prefs_ = prefs;
  daily_ = std::make_unique<ArcDailyMetrics>(prefs);
}

void ArcMetricsService::ReportProvisioningStartTime(
    const base::TimeTicks& start_time,
    const std::string& account_type_suffix) {
  arc_provisioning_start_time_.emplace(start_time);
  arc_provisioning_account_type_suffix_.emplace(account_type_suffix);
}

void ArcMetricsService::ReportProvisioningPreSignIn() {
  if (arc_provisioning_start_time_.has_value() &&
      arc_provisioning_account_type_suffix_.has_value()) {
    base::UmaHistogramCustomTimes(
        kProvisioningPreSignInTimeDelta +
            arc_provisioning_account_type_suffix_.value(),
        base::TimeTicks::Now() - arc_provisioning_start_time_.value(),
        base::Seconds(1), base::Minutes(5), 50);
    arc_provisioning_start_time_.reset();
    arc_provisioning_account_type_suffix_.reset();
  } else {
    LOG(ERROR) << "PreSignIn reported without prior starting time";
  }
}

void ArcMetricsService::ReportWaylandLateTimingEvent(
    mojom::WaylandTimingEvent event,
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::string suffix = WaylandTimingEventToString(event);
  base::UmaHistogramLongTimes("Arc.Wayland.LateTiming.Duration" + suffix,
                              duration);
  base::UmaHistogramEnumeration("Arc.Wayland.LateTiming.Event", event);
}

void ArcMetricsService::ReportWebViewProcessStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(prefs_);
  prefs_->SetBoolean(prefs::kWebViewProcessStarted, true);
}

void ArcMetricsService::ReportNewQosSocketCount(int count) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramCounts100000("Arc.Net.Qos.NewQosSocketCount", count);
}

void ArcMetricsService::ReportQosSocketPercentage(int perc) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramCounts100("Arc.Net.Qos.QosSocketPercentage", perc);
}

void ArcMetricsService::ReportArcKeyMintError(mojom::ArcKeyMintError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.KeyMint.KeyMintError", error);
}

void ArcMetricsService::ReportDragResizeLatency(
    const std::vector<base::TimeDelta>& durations) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto duration : durations) {
    base::UmaHistogramCustomTimes("Arc.WM.WindowDragResizeTime", duration,
                                  /*minimum=*/base::Milliseconds(1),
                                  /*maximum=*/base::Seconds(3), 100);
  }
}

void ArcMetricsService::ReportAppErrorDialogType(
    mojom::AppErrorDialogType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.WM.AppErrorDialog.Type", type);
}

void ArcMetricsService::ReportApkCacheHit(bool hit) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramBoolean("Arc.AppInstall.CacheHit", hit);
}

void ArcMetricsService::ReportAppCategoryDataSizeList(
    std::vector<mojom::AppCategoryDataSizePtr> list) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& item : list) {
    const std::string metrics =
        base::StringPrintf("Arc.Data.AppCategory.%s.DataSize",
                           AndroidAppCategoryToString(item->category));
    base::UmaHistogramCustomCounts(metrics, item->data_size_in_mb,
                                   kUmaDataSizeInMBMin, kUmaDataSizeInMBMax,
                                   kUmaNumBuckets);
  }
}

void ArcMetricsService::ReportDataDirectorySizeList(
    std::vector<mojom::DataDirectorySizePtr> list) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& item : list) {
    const std::string metrics = base::StringPrintf(
        "Arc.Data.Dir.%s.Size", AndroidDataDirectoryToString(item->directory));
    base::UmaHistogramCustomCounts(metrics, item->size_in_mb,
                                   kUmaDataSizeInMBMin, kUmaDataSizeInMBMax,
                                   kUmaNumBuckets);
  }
}

void ArcMetricsService::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  was_arc_window_active_ = ash::IsArcWindow(gained_active);
  if (!was_arc_window_active_) {
    gamepad_interaction_recorded_ = false;
    return;
  }
  RecordArcUserInteraction(UserInteractionType::APP_CONTENT_WINDOW_INTERACTION);
}

void ArcMetricsService::OnGamepadEvent(const ui::GamepadEvent& event) {
  if (!was_arc_window_active_)
    return;
  if (gamepad_interaction_recorded_)
    return;
  gamepad_interaction_recorded_ = true;
  RecordArcUserInteraction(UserInteractionType::GAMEPAD_INTERACTION);
}

void ArcMetricsService::OnTaskCreated(int32_t task_id,
                                      const std::string& package_name,
                                      const std::string& activity,
                                      const std::string& intent) {
  task_ids_.push_back(task_id);
  guest_os_engagement_metrics_.SetBackgroundActive(true);
}

void ArcMetricsService::OnTaskDestroyed(int32_t task_id) {
  auto it = base::ranges::find(task_ids_, task_id);
  if (it == task_ids_.end()) {
    LOG(WARNING) << "unknown task_id, background time might be undermeasured";
    return;
  }
  task_ids_.erase(it);
  guest_os_engagement_metrics_.SetBackgroundActive(!task_ids_.empty());
}

void ArcMetricsService::OnArcStarted() {
  DCHECK(prefs_);

  metrics_anr_ = std::make_unique<ArcMetricsAnr>(prefs_);

  // Post tasks to record load average.
  for (size_t index = 0; index < std::size(kLoadAverageHistograms); ++index) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ArcMetricsService::MeasureLoadAverage,
                       weak_ptr_factory_.GetWeakPtr(), index),
        kLoadAverageHistograms[index].duration);
  }
}

void ArcMetricsService::OnArcSessionStopped() {
  DCHECK(prefs_);

  boot_type_ = mojom::BootType::UNKNOWN;
  metrics_anr_.reset();

  base::UmaHistogramBoolean("Arc.Session.HasWebViewUsage",
                            prefs_->GetBoolean(prefs::kWebViewProcessStarted));
  prefs_->SetBoolean(prefs::kWebViewProcessStarted, false);
}

void ArcMetricsService::AddAppKillObserver(AppKillObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  app_kill_observers_.AddObserver(obs);
}

void ArcMetricsService::RemoveAppKillObserver(AppKillObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  app_kill_observers_.RemoveObserver(obs);
}

void ArcMetricsService::AddUserInteractionObserver(
    UserInteractionObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  user_interaction_observers_.AddObserver(obs);
}

void ArcMetricsService::RemoveUserInteractionObserver(
    UserInteractionObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  user_interaction_observers_.RemoveObserver(obs);
}

void ArcMetricsService::AddBootTypeObserver(BootTypeObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  boot_type_observers_.AddObserver(obs);
}

void ArcMetricsService::RemoveBootTypeObserver(BootTypeObserver* obs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  boot_type_observers_.RemoveObserver(obs);
}

std::optional<base::TimeTicks> ArcMetricsService::GetArcStartTimeFromEvents(
    std::vector<mojom::BootProgressEventPtr>& events) {
  mojom::BootProgressEventPtr arc_upgraded_event;
  for (auto it = events.begin(); it != events.end(); ++it) {
    if (!(*it)->event.compare(kBootProgressArcUpgraded)) {
      arc_upgraded_event = std::move(*it);
      events.erase(it);
      return base::Milliseconds(arc_upgraded_event->uptimeMillis) +
             base::TimeTicks();
    }
  }
  return std::nullopt;
}

void ArcMetricsService::ReportMemoryPressureArcVmKills(int count,
                                                       int estimated_freed_kb) {
  for (auto& obs : app_kill_observers_)
    obs.OnArcMemoryPressureKill(count, estimated_freed_kb);
}

void ArcMetricsService::ReportArcNetworkEvent(mojom::ArcNetworkEvent event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.Net.ArcNetworkEvent", event);
}

void ArcMetricsService::ReportArcNetworkError(mojom::ArcNetworkError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.Net.ArcNetworkError", error);
}

void ArcMetricsService::MeasureLoadAverage(size_t index) {
  struct sysinfo info = {};
  if (sysinfo(&info) < 0) {
    DCHECK_LT(index, std::size(kLoadAverageHistograms));
    PLOG(ERROR) << "sysinfo() failed when trying to record "
                << kLoadAverageHistograms[index].name;
    return;
  }
  DCHECK_LT(index, std::size(info.loads));
  // Load average values returned by sysinfo() are scaled up by
  // 1 << SI_LOAD_SHIFT.
  const int loadx100 = info.loads[index] * 100 / (1 << SI_LOAD_SHIFT);
  // _SC_NPROCESSORS_ONLN instead of base::SysInfo::NumberOfProcessors() which
  // uses _SC_NPROCESSORS_CONF to get the number of online processors in case
  // some cores are disabled.
  const int loadx100_per_processor = loadx100 / sysconf(_SC_NPROCESSORS_ONLN);
  load_averages_after_arc_start_[index] = loadx100_per_processor;

  MaybeRecordLoadAveragePerProcessor();
}

void ArcMetricsService::MaybeRecordLoadAveragePerProcessor() {
  if (boot_type_ == mojom::BootType::UNKNOWN) {
    // Not ready because the boot type is still unknown.
    return;
  }
  for (const auto& value : load_averages_after_arc_start_) {
    const size_t index = value.first;
    const int loadx100_per_processor = value.second;
    DCHECK_LT(index, std::size(kLoadAverageHistograms));
    base::UmaHistogramCustomCounts(
        kLoadAverageHistograms[index].name + BootTypeToString(boot_type_),
        loadx100_per_processor, 0, 5000, 50);
  }
  // Erase the values to avoid recording them again.
  load_averages_after_arc_start_.clear();
}

ArcMetricsService::ProcessObserver::ProcessObserver(
    ArcMetricsService* arc_metrics_service)
    : arc_metrics_service_(arc_metrics_service) {}

ArcMetricsService::ProcessObserver::~ProcessObserver() = default;

void ArcMetricsService::ProcessObserver::OnConnectionReady() {
  arc_metrics_service_->OnProcessConnectionReady();
}

void ArcMetricsService::ProcessObserver::OnConnectionClosed() {
  arc_metrics_service_->OnProcessConnectionClosed();
}

ArcMetricsService::ArcBridgeServiceObserver::ArcBridgeServiceObserver() =
    default;

ArcMetricsService::ArcBridgeServiceObserver::~ArcBridgeServiceObserver() =
    default;

void ArcMetricsService::ArcBridgeServiceObserver::BeforeArcBridgeClosed() {
  arc_bridge_closing_ = true;
}
void ArcMetricsService::ArcBridgeServiceObserver::AfterArcBridgeClosed() {
  arc_bridge_closing_ = false;
}

ArcMetricsService::IntentHelperObserver::IntentHelperObserver(
    ArcMetricsService* arc_metrics_service,
    ArcBridgeServiceObserver* arc_bridge_service_observer)
    : arc_metrics_service_(arc_metrics_service),
      arc_bridge_service_observer_(arc_bridge_service_observer) {}

ArcMetricsService::IntentHelperObserver::~IntentHelperObserver() = default;

void ArcMetricsService::IntentHelperObserver::OnConnectionClosed() {
  // Ignore closed connections due to the container shutting down.
  if (!arc_bridge_service_observer_->arc_bridge_closing_) {
    DCHECK(arc_metrics_service_->histogram_namer_cb_);
    LogStabilityUmaEnum(arc_metrics_service_->histogram_namer_cb_.Run(
                            "Arc.Session.MojoDisconnection"),
                        MojoConnectionType::INTENT_HELPER);
  }
}

ArcMetricsService::AppLauncherObserver::AppLauncherObserver(
    ArcMetricsService* arc_metrics_service,
    ArcBridgeServiceObserver* arc_bridge_service_observer)
    : arc_metrics_service_(arc_metrics_service),
      arc_bridge_service_observer_(arc_bridge_service_observer) {}

ArcMetricsService::AppLauncherObserver::~AppLauncherObserver() = default;

void ArcMetricsService::AppLauncherObserver::OnConnectionClosed() {
  // Ignore closed connections due to the container shutting down.
  if (!arc_bridge_service_observer_->arc_bridge_closing_) {
    DCHECK(arc_metrics_service_->histogram_namer_cb_);
    LogStabilityUmaEnum(arc_metrics_service_->histogram_namer_cb_.Run(
                            "Arc.Session.MojoDisconnection"),
                        MojoConnectionType::APP_LAUNCHER);
  }
}

}  // namespace arc
