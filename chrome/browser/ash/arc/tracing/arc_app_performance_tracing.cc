// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Tracing delay for jankinees.
constexpr base::TimeDelta kJankinessTracingTime = base::Minutes(5);

// Minimum number of frames for a jankiness tracing result to be valid.
constexpr int kMinTotalFramesJankiness = 1000;

// Singleton factory for ArcAppPerformanceTracing.
class ArcAppPerformanceTracingFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAppPerformanceTracing,
          ArcAppPerformanceTracingFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAppPerformanceTracingFactory";

  static ArcAppPerformanceTracingFactory* GetInstance() {
    return base::Singleton<ArcAppPerformanceTracingFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAppPerformanceTracingFactory>;
  ArcAppPerformanceTracingFactory() {
    DependsOn(ArcAppListPrefsFactory::GetInstance());
    // TODO(crbug.com/40227318): This should probably depend on SyncService.
  }
  ~ArcAppPerformanceTracingFactory() override = default;
};

// Singleton for app id to category mapping.
class AppToCategoryMapper {
 public:
  AppToCategoryMapper() {
    // Please refer to
    // https://goto.google.com/arc++app-runtime-performance-metrics.
    Add("iicceeckdelepgbcpojbgahbhnklpane", "Roblox");
    Add("ojjlibnpojmhhabohpkclejfdblglkpj", "MinecraftEducationEdition");
    Add("hhkmajjdndhdnkbmomodobajdjngeejb", "GachaClub");
    Add("niajncocfieigpbiamllekeadpgbhkke", "GarenaFreeFire");
    Add("icloenboalgjkknjdficgpgpcedmmojn", "Netflix");
    Add("nlhkolcnehphdkaljhgcbkmahloeacoj", "PUBGMobile");
    Add("gihmggjjlnjaldngedmnegjmhccccahg", "MinecraftConsumerEdition");
    Add("aejndhminbfocgmlbmmccankkembehmc", "AmongUs");
    Add("lbefcdhjbnilmnokeflglbaiaebadckd", "RaidLegends");
    Add("bifaabbnnccaenolhjngemgmegdjflkg", "Underlords");
    Add("indkdfghopoafaifcjbnonjkgdjnbhli", "TocaLife");
    Add("kmglgjicdcmjphkoojighlhjejkiefih", "CandyCrush");
    Add("ckkdolbnmedndlibioieibdjnifacikn", "Homescapes");
    Add("hpnpilodeljgmlapcmaaachbolchfcmh", "FIFAMobile");
    Add("fkhbcehgdndojcdlkhkihhhnhipkgddd", "GenshinImpact");
  }

  static AppToCategoryMapper& GetInstance() {
    static base::NoDestructor<AppToCategoryMapper> instance;
    return *instance.get();
  }

  AppToCategoryMapper(const AppToCategoryMapper&) = delete;
  AppToCategoryMapper& operator=(const AppToCategoryMapper&) = delete;

  // Returns empty string if category is not set for app |app_id|.
  const std::string& GetCategory(const std::string& app_id) const {
    const auto& it = app_id_to_category_.find(app_id);
    if (it == app_id_to_category_.end()) {
      return base::EmptyString();
    }
    return it->second;
  }

  void Add(const std::string& app_id, const std::string& category) {
    app_id_to_category_[app_id] = category;
  }

 private:
  ~AppToCategoryMapper() = default;

  std::map<std::string, std::string> app_id_to_category_;
};

}  // namespace

struct ArcAppPerformanceTracing::ActiveTask {
  ActiveTask(exo::Surface* root_surface, exo::SurfaceObserver* observer, int id)
      : root_surface(root_surface, observer), id(id) {}

  // Used for automatic observer adding/removing.
  exo::ScopedSurface root_surface;

  // ARC task id of the window.
  const int id;
};

ArcAppPerformanceTracing::ArcAppPerformanceTracing(
    content::BrowserContext* context,
    ArcBridgeService* bridge)
    : context_(context), weak_ptr_factory_(this) {
  // Not related tests may indirectly create this instance and helper might
  // not be set.
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  }
  ArcAppListPrefs::Get(context_)->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

// Releasing resources in DTOR is not safe, see |Shutdown|.
ArcAppPerformanceTracing::~ArcAppPerformanceTracing() = default;

// static
ArcAppPerformanceTracing* ArcAppPerformanceTracing::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAppPerformanceTracingFactory::GetForBrowserContext(context);
}

// static
ArcAppPerformanceTracing*
ArcAppPerformanceTracing::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcAppPerformanceTracingFactory::GetForBrowserContextForTesting(
      context);
}

// static
void ArcAppPerformanceTracing::SetFocusAppForTesting(
    const std::string& package_name,
    const std::string& activity,
    const std::string& category) {
  AppToCategoryMapper::GetInstance().Add(
      ArcAppListPrefs::GetAppId(package_name, activity), category);
}

void ArcAppPerformanceTracing::SetCustomSessionReadyCallbackForTesting(
    CustomSessionReadyCallback callback) {
  custom_session_ready_callback_ = std::move(callback);
}

void ArcAppPerformanceTracing::MaybeCancelTracing() {
  jankiness_timer_.Stop();
  session_.reset();
}

void ArcAppPerformanceTracing::Shutdown() {
  display::Screen::GetScreen()->RemoveObserver(this);

  MaybeCancelTracing();

  // |session_|. Make sure that |active_window_| is detached.
  DetachActiveWindow();

  ArcAppListPrefs::Get(context_)->RemoveObserver(this);
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
  }
}

void ArcAppPerformanceTracing::OnCustomTraceDone(
    const std::optional<PerfTraceResult>& result) {
  bool success = result.has_value();

  // TODO(b/318754606): commitDeviation is still used by tast-test clients, only
  // switch from commitDeviation to presentDeviation (and fps to perceivedFps)
  // once the it's fixed to not output 0 FPS on display-less Chromebox devices.
  custom_trace_result_.emplace(
      base::Value::Dict()
          .Set("success", success)
          .Set("fps", success ? result->fps : 0)
          .Set("perceivedFps", success ? result->perceived_fps : 0)
          .Set("commitDeviation", success ? result->commit_deviation : 0)
          .Set("presentDeviation", success ? result->present_deviation : 0)
          .Set("renderQuality", success ? result->render_quality : 0)
          .Set("janksPerMinute", success ? result->janks_per_minute : 0)
          .Set("janksPercentage", success ? result->janks_percentage : 0));
}

bool ExpectingPresentEvents() {
  auto* screen = display::Screen::GetScreen();

  return screen->GetNumDisplays() > 1 || screen->GetPrimaryDisplay().detected();
}

bool ArcAppPerformanceTracing::StartCustomTracing() {
  if (!active_window_) {
    return false;
  }

  session_ = std::make_unique<ArcAppPerformanceTracingSession>(
      active_window_, *ticks_now_callback());

  // Disable listening for presents if we don't have an attached display.
  // See b/332726656
  session_->set_trace_real_presents(ExpectingPresentEvents());

  custom_trace_result_.reset();
  session_->Schedule(
      false /* detect_idles */, base::TimeDelta() /* start_delay */,
      base::TimeDelta() /* tracing_period */,
      base::BindOnce(&ArcAppPerformanceTracing::OnCustomTraceDone,
                     weak_ptr_factory_.GetWeakPtr()));
  if (custom_session_ready_callback_) {
    custom_session_ready_callback_.Run();
  }

  return true;
}

base::Value::Dict ArcAppPerformanceTracing::StopCustomTracing() {
  custom_trace_result_.reset();
  if (session_ && session_->tracing_active()) {
    session_->Finish();
    DCHECK(custom_trace_result_.has_value());
  }

  if (!custom_trace_result_.has_value()) {
    OnCustomTraceDone(std::nullopt);
  }

  return *std::move(custom_trace_result_);
}

void ArcAppPerformanceTracing::OnWindowActivated(ActivationReason reason,
                                                 aura::Window* gained_active,
                                                 aura::Window* lost_active) {
  // Discard any active tracing if any.
  session_.reset();

  // Stop and report previous active window's jankiness tracing so far.
  FinalizeJankinessTracing(true /* stopped_early */);

  // Detach previous active window if it is set.
  DetachActiveWindow();

  if (!gained_active) {
    return;
  }

  active_window_ = gained_active;
  active_window_->AddObserver(this);
  TrackIfTaskIsActive();

  if (!active_task_ && !GetWindowSessionId(gained_active)) {
    // No need to observe if this is not a task or an ARC ghost window.
    DetachActiveWindow();
  }
}

void ArcAppPerformanceTracing::TrackIfTaskIsActive() {
  DCHECK(active_window_);

  if (active_task_) {
    return;
  }

  auto task_id = arc::GetWindowTaskId(active_window_);
  if (!task_id) {
    // Not a task window, so is not traceable, but if this is a ghost window,
    // it may be traceable later.
    return;
  }

  exo::Surface* const surface = exo::GetShellRootSurface(active_window_);
  // Should never happen, but check against a task with unset root surface.
  if (!surface) {
    return;
  }

  // Observe active ARC++ window.
  // Use scoped surface observer to be safe on the surface
  // destruction. |exo::GetShellRootSurface| would fail in case
  // the surface gets destroyed before widget.
  active_task_ =
      std::make_unique<ActiveTask>(surface, this /* observer */, *task_id);

  StartJankinessTracing();

  MaybeStartTracing();
}

void ArcAppPerformanceTracing::OnWindowDestroying(aura::Window* window) {
  // ARC++ window will be destroyed.
  DCHECK_EQ(active_window_, window);

  MaybeCancelTracing();

  DetachActiveWindow();
}

void ArcAppPerformanceTracing::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  if (key == exo::kApplicationIdKey) {
    TrackIfTaskIsActive();
  }
}

void ArcAppPerformanceTracing::OnTaskCreated(int32_t task_id,
                                             const std::string& package_name,
                                             const std::string& activity,
                                             const std::string& intent,
                                             int32_t session_id) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  task_id_to_app_id_[task_id] = std::make_pair(app_id, package_name);
  MaybeStartTracing();
}

void ArcAppPerformanceTracing::OnTaskDestroyed(int32_t task_id) {
  task_id_to_app_id_.erase(task_id);
}

void ArcAppPerformanceTracing::StartJankinessTracing() {
  DCHECK(!jankiness_timer_.IsRunning());
  jankiness_timer_.Start(
      FROM_HERE, kJankinessTracingTime,
      base::BindOnce(&ArcAppPerformanceTracing::FinalizeJankinessTracing,
                     base::Unretained(this), false /* stopped_early */));
}

void ArcAppPerformanceTracing::HandleActiveAppRendered(base::Time timestamp) {
  DCHECK(active_task_);

  const std::string& app_id = task_id_to_app_id_[active_task_->id].first;
  const base::Time launch_request_time =
      ArcAppListPrefs::Get(context_)->PollLaunchRequestTime(app_id);
  if (!launch_request_time.is_null()) {
    base::UmaHistogramTimes(
        "Arc.Runtime.Performance.Generic.FirstFrameRendered",
        timestamp - launch_request_time);
  }
}

void ArcAppPerformanceTracing::OnCommit(exo::Surface* surface) {
  HandleActiveAppRendered(base::Time::Now());
  // Only need first frame. We don't need to observe anymore.
  surface->RemoveSurfaceObserver(this);
}

void ArcAppPerformanceTracing::OnSurfaceDestroying(exo::Surface* surface) {
  // |scoped_surface_| might be already reset in case window is destroyed
  // first.
  DCHECK(!active_task_ || (active_task_->root_surface.get() == surface));
  DetachActiveWindow();
}

// This method is invoked when a display changes between detected and not
// detected. One can test manually on a Chromebook by turning off
// sleep-on-lid-close in the Power settings and closing the lid during a trace.
// Jankiness tracing is not affected, as this does not rely on frame present
// events, and uses internal Android tracing.
void ArcAppPerformanceTracing::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!ExpectingPresentEvents()) {
    session_.reset();
  } else {
    MaybeStartTracing();
  }
}

void ArcAppPerformanceTracing::FinalizeJankinessTracing(bool stopped_early) {
  // Never started. Nothing to do.
  if (!jankiness_timer_.IsRunning() && stopped_early) {
    return;
  }

  jankiness_timer_.Stop();

  // Check if we have all conditions met, ARC++ window is active and information
  // is available for associated task.
  if (!active_task_) {
    return;
  }

  const auto it = task_id_to_app_id_.find(active_task_->id);
  if (it == task_id_to_app_id_.end()) {
    // It is normal that information might not be available at this time.
    return;
  }

  // Test instances might not have Service Manager running.
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager) {
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->metrics(), GetGfxMetrics);
  if (!instance) {
    return;
  }

  const std::string package_name = it->second.second;
  auto callback = base::BindOnce(&ArcAppPerformanceTracing::OnGfxMetrics,
                                 base::Unretained(this), package_name);
  instance->GetGfxMetrics(package_name, std::move(callback));

  // Finalized normally, safe to restart.
  if (!stopped_early) {
    StartJankinessTracing();
  }
}

void ArcAppPerformanceTracing::OnGfxMetrics(const std::string& package_name,
                                            mojom::GfxMetricsPtr metrics) {
  if (!metrics) {
    LOG(ERROR) << "Failed to resolve GFX metrics";
    return;
  }

  uint64_t framesTotal = metrics->framesTotal;
  uint64_t framesJanky = metrics->framesJanky;
  const uint32_t frameTime95 = metrics->frameTimePercentile95;  // in ms.

  const auto it = package_name_to_gfx_metrics_.find(package_name);
  const bool first_time = it == package_name_to_gfx_metrics_.end();

  // Cached data exists and not outdated. Calculate delta.
  if (!first_time && it->second.framesTotal <= framesTotal) {
    framesTotal -= it->second.framesTotal;
    framesJanky -= it->second.framesJanky;
  }

  // Update cache.
  package_name_to_gfx_metrics_[package_name] = *metrics;

  // Not enough data.
  if (framesTotal < kMinTotalFramesJankiness) {
    VLOG(1) << "Not enough GFX metrics data collected to report.";
    return;
  }

  // We can only calculate real numbers for initial data. Only report if first
  // time.
  if (first_time) {
    const base::TimeDelta frameTime = base::Milliseconds(frameTime95);
    base::UmaHistogramTimes("Arc.Runtime.Performance.Generic.FrameTime",
                            frameTime);
    VLOG(1) << "Total Frames: " << framesTotal << " | "
            << "Janky Frames: " << framesJanky << " | "
            << "95 Percentile Frame Time: " << frameTime.InMilliseconds()
            << "ms";
  } else {
    VLOG(1) << "Total Frames: " << framesTotal << " | "
            << "Janky Frames: " << framesJanky;
  }

  const int jankiness = (framesJanky * 100) / framesTotal;

  base::UmaHistogramPercentage("Arc.Runtime.Performance.Generic.Jankiness",
                               jankiness);
}

void ArcAppPerformanceTracing::MaybeStartTracing() {
  if (session_) {
    // We are already tracing, ignore.
    DCHECK_EQ(session_->window(), active_window_);
    return;
  }

  // Check if we have all conditions met, ARC++ window is active and information
  // is available for associated task.
  if (!active_task_) {
    return;
  }

  if (!ExpectingPresentEvents()) {
    return;
  }

  const auto it = task_id_to_app_id_.find(active_task_->id);
  if (it == task_id_to_app_id_.end()) {
    // It is normal that information might not be available at this time.
    return;
  }

  const std::string& category = AppToCategoryMapper::GetInstance().GetCategory(
      it->second.first /* app_id */);

  if (category.empty()) {
    // App is not recognized as app for tracing, ignore it.
    return;
  }

  Profile* const profile = Profile::FromBrowserContext(context_);
  DCHECK(profile);

  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    // Possible if sync is disabled by command line flag.
    // TODO(crbug.com/40227318): This should probably handled by
    // ArcAppPerformanceTracingFactory.
    VLOG(1) << "Cannot trace: Sync service not available";
    return;
  }

  const syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();

  const bool apps_sync_enabled = sync_service->IsSyncFeatureEnabled() &&
                                 sync_user_settings->GetSelectedOsTypes().Has(
                                     syncer::UserSelectableOsType::kOsApps);

  if (!apps_sync_enabled) {
    VLOG(1) << "Cannot trace: App Sync is not enabled.";
    return;
  }

  if (sync_user_settings->IsUsingExplicitPassphrase()) {
    VLOG(1) << "Cannot trace: User has a sync passphrase.";
    return;
  }

  session_ = std::make_unique<ArcAppPerformanceTracingSession>(
      active_window_, *ticks_now_callback());
  reporting_.Schedule(session_.get(), category);
}

void ArcAppPerformanceTracing::DetachActiveWindow() {
  if (!active_window_) {
    return;
  }

  active_task_.reset();
  active_window_->RemoveObserver(this);
  active_window_ = nullptr;
}

// static
void ArcAppPerformanceTracing::EnsureFactoryBuilt() {
  ArcAppPerformanceTracingFactory::GetInstance();
}

// static
TicksNowCallback* ArcAppPerformanceTracing::ticks_now_callback() {
  static base::NoDestructor<TicksNowCallback> storage{
      base::BindRepeating(&base::TimeTicks::Now)};
  return storage.get();
}

// static
void ArcAppPerformanceTracing::reset_ticks_now_callback() {
  *ticks_now_callback() = base::BindRepeating(&base::TimeTicks::Now);
}

}  // namespace arc
