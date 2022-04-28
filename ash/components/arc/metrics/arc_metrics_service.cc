// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_metrics_service.h"

#include <sys/sysinfo.h>

#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_anr.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/exo/wm_helper.h"
#include "components/metrics/psi_memory_parser.h"
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
constexpr int kUmaFixupDirectoriesCountMin = 0;
constexpr int kUmaFixupDirectoriesCountMax = 5000000;
constexpr int kUmaFixupAppsCountMin = 0;
constexpr int kUmaFixupAppsCountMax = 10000;

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
  NOTREACHED();
  return "";
}

const char* LowLatencyStylusLibraryTypeToString(
    mojom::LowLatencyStylusLibraryType library_type) {
  switch (library_type) {
    case mojom::LowLatencyStylusLibraryType::kUnsupported:
      break;
    case mojom::LowLatencyStylusLibraryType::kCPU:
      return ".CPU";
    case mojom::LowLatencyStylusLibraryType::kGPU:
      return ".GPU";
  }
  NOTREACHED();
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
  return "";
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
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/748380): Fix shutdown order.
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

void ArcMetricsService::SetHistogramNamer(HistogramNamer histogram_namer) {
  histogram_namer_ = histogram_namer;
}

void ArcMetricsService::OnProcessConnectionReady() {
  VLOG(2) << "Start updating process list.";
  request_process_list_timer_.Start(FROM_HERE, kRequestProcessListPeriod, this,
                                    &ArcMetricsService::RequestProcessList);

  if (IsArcVmEnabled()) {
    // Initialize prev_logged_memory_kills_ by immediately requesting new
    // values.
    prev_logged_memory_kills_.reset();
    RequestLowMemoryKillCounts();
    request_kill_count_timer_.Start(
        FROM_HERE, kRequestKillCountPeriod, this,
        &ArcMetricsService::RequestLowMemoryKillCounts);
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

void ArcMetricsService::RequestLowMemoryKillCountsForTesting() {
  RequestLowMemoryKillCounts();
}

void ArcMetricsService::RequestLowMemoryKillCounts() {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestLowMemoryKillCounts);
  if (!process_instance) {
    LOG(WARNING)
        << "Cannot get ProcessInstance for method RequestLowMemoryKillCounts";
    return;
  }
  process_instance->RequestLowMemoryKillCounts(
      base::BindOnce(&ArcMetricsService::LogLowMemoryKillCounts,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Helper that logs a single kill count type to a histogram, or logs a warning
// if the counter decreased.
static void LogLowMemoryKillCount(const char* suffix,
                                  uint32_t prev_kills,
                                  uint32_t curr_kills) {
  if (prev_kills <= curr_kills) {
    std::string name = std::string("Arc.App.LowMemoryKills.") + suffix;
    base::UmaHistogramExactLinear(name, curr_kills - prev_kills, 50);
  } else {
    LOG(WARNING) << "LowMemoryKillCounts reported a decrease for " << suffix
                 << ", previous: " << prev_kills << ", current: " << curr_kills;
  }
}

void ArcMetricsService::LogLowMemoryKillCounts(
    mojom::LowMemoryKillCountsPtr counts) {
  if (prev_logged_memory_kills_) {
    // Only log to the histograms if we have a previous sample to compute deltas
    // from.
    LogLowMemoryKillCount("LinuxOOMCount10Minutes",
                          prev_logged_memory_kills_->guest_oom,
                          counts->guest_oom);
    LogLowMemoryKillCount("LMKD.ForegroundCount10Minutes",
                          prev_logged_memory_kills_->lmkd_foreground,
                          counts->lmkd_foreground);
    LogLowMemoryKillCount("LMKD.PerceptibleCount10Minutes",
                          prev_logged_memory_kills_->lmkd_perceptible,
                          counts->lmkd_perceptible);
    LogLowMemoryKillCount("LMKD.CachedCount10Minutes",
                          prev_logged_memory_kills_->lmkd_cached,
                          counts->lmkd_cached);
    LogLowMemoryKillCount("Pressure.ForegroundCount10Minutes",
                          prev_logged_memory_kills_->pressure_foreground,
                          counts->pressure_foreground);
    LogLowMemoryKillCount("Pressure.PerceptibleCount10Minutes",
                          prev_logged_memory_kills_->pressure_perceptible,
                          counts->pressure_perceptible);
    LogLowMemoryKillCount("Pressure.CachedCount10Minutes",
                          prev_logged_memory_kills_->pressure_cached,
                          counts->pressure_cached);
  }
  prev_logged_memory_kills_ = std::move(counts);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type,
    absl::optional<base::TimeTicks> arc_start_time) {
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
    const std::string name = "Arc." + event->event + suffix;
    const base::TimeTicks uptime =
        base::Milliseconds(event->uptimeMillis) + base::TimeTicks();
    const base::TimeDelta elapsed_time = uptime - arc_start_time.value();
    base::UmaHistogramCustomTimes(name, elapsed_time, kUmaMinTime, kUmaMaxTime,
                                  kUmaNumBuckets);
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

  if (IsArcVmEnabled()) {
    // For VM builds, do not call into session_manager since we don't use it
    // for the builds. The upgrade time is included in the events vector so we
    // can extract it here.
    absl::optional<base::TimeTicks> arc_start_time =
        GetArcStartTimeFromEvents(events);
    OnArcStartTimeRetrieved(std::move(events), boot_type, arc_start_time);
    return;
  }

  // Retrieve ARC full container's start time from session manager.
  chromeos::SessionManagerClient::Get()->GetArcStartTime(base::BindOnce(
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

void ArcMetricsService::ReportImageCopyPasteCompatAction(
    mojom::ArcImageCopyPasteCompatAction action_type) {
  base::UmaHistogramEnumeration("Arc.ImageCopyPasteCompatOperationType",
                                action_type);
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
    absl::optional<base::TimeTicks> arc_start_time) {
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
  chromeos::SessionManagerClient::Get()->GetArcStartTime(
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

void ArcMetricsService::ReportClipboardDragDropEvent(
    mojom::ArcClipboardDragDropEvent event_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Arc.ClipboardDragDrop", event_type);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UMA_HISTOGRAM_ENUMERATION("Arc.LowLatencyStylusLibraryApisCounter", api_id);
}

void ArcMetricsService::ReportLowLatencyStylusLibPredictionTarget(
    mojom::LowLatencyStylusLibPredictionTargetPtr prediction_target) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramCounts100(
      base::StrCat(
          {"Arc.LowLatencyStylusLibrary.PredictionTarget",
           LowLatencyStylusLibraryTypeToString(prediction_target->type)}),
      prediction_target->target);
}

void ArcMetricsService::ReportEntireFixupMetrics(base::TimeDelta duration,
                                                 uint32_t number_of_directories,
                                                 uint32_t number_of_failures) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramLongTimes("Arc.Fixup.Entire.Duration", duration);
  base::UmaHistogramCustomCounts("Arc.Fixup.Entire.Directories",
                                 number_of_directories,
                                 kUmaFixupDirectoriesCountMin,
                                 kUmaFixupDirectoriesCountMax, kUmaNumBuckets);
  base::UmaHistogramCustomCounts("Arc.Fixup.Entire.Failures",
                                 number_of_failures, kUmaFixupAppsCountMin,
                                 kUmaFixupAppsCountMax, kUmaNumBuckets);
}

void ArcMetricsService::ReportPerAppFixupMetrics(
    base::TimeDelta duration,
    uint32_t number_of_directories) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramLongTimes("Arc.Fixup.PerApp.Duration", duration);
  base::UmaHistogramCustomCounts("Arc.Fixup.PerApp.Directories",
                                 number_of_directories,
                                 kUmaFixupDirectoriesCountMin,
                                 kUmaFixupDirectoriesCountMax, kUmaNumBuckets);
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

  auto stat = psi_parser_->ParseMetrics(psi_file_contents.data(),
                                        psi_file_contents.size(), &metric_some,
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

void ArcMetricsService::ReportWaylandLateTimingDuration(
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramLongTimes("Arc.Wayland.LateTiming.Duration", duration);
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
  auto it = std::find(task_ids_.begin(), task_ids_.end(), task_id);
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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ArcMetricsService::MeasureLoadAverage,
                       weak_ptr_factory_.GetWeakPtr(), index),
        kLoadAverageHistograms[index].duration);
  }
}

void ArcMetricsService::OnArcSessionStopped() {
  boot_type_ = mojom::BootType::UNKNOWN;
  metrics_anr_.reset();
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

absl::optional<base::TimeTicks> ArcMetricsService::GetArcStartTimeFromEvents(
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
  return absl::nullopt;
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
    LogStabilityUmaEnum(arc_metrics_service_->histogram_namer_.Run(
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
    LogStabilityUmaEnum(arc_metrics_service_->histogram_namer_.Run(
                            "Arc.Session.MojoDisconnection"),
                        MojoConnectionType::APP_LAUNCHER);
  }
}

}  // namespace arc
