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
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_custom_session.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_uma_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/aura/window.h"

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
    // TODO(crbug.com/1330894): This should probably depend on SyncService.
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
    if (it == app_id_to_category_.end())
      return base::EmptyString();
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

ArcAppPerformanceTracing::ArcAppPerformanceTracing(
    content::BrowserContext* context,
    ArcBridgeService* bridge)
    : context_(context) {
  // Not related tests may indirectly create this instance and helper might
  // not be set.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  ArcAppListPrefs::Get(context_)->AddObserver(this);
}

// Releasing resources in DTOR is not safe, see |Shutdown|.
ArcAppPerformanceTracing::~ArcAppPerformanceTracing() {
  if (arc_active_window_) {
    exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
    // Surface might be destroyed.
    if (surface)
      surface->RemoveSurfaceObserver(this);
  }
}

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

void ArcAppPerformanceTracing::Shutdown() {
  CancelJankinessTracing();

  MaybeStopTracing();

  // |session_|. Make sure that |arc_active_window_| is detached.
  DetachActiveWindow();

  ArcAppListPrefs::Get(context_)->RemoveObserver(this);
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
}

bool ArcAppPerformanceTracing::StartCustomTracing() {
  if (!arc_active_window_)
    return false;

  session_ = std::make_unique<ArcAppPerformanceTracingCustomSession>(this);
  session_->Schedule();
  if (custom_session_ready_callback_)
    custom_session_ready_callback_.Run();

  return true;
}

void ArcAppPerformanceTracing::StopCustomTracing(
    ResultCallback result_callback) {
  if (!session_ || !session_->AsCustomSession()) {
    std::move(result_callback).Run(false /* success */, 0, 0, 0);
    return;
  }

  session_->AsCustomSession()->StopAndAnalyze(std::move(result_callback));
}

void ArcAppPerformanceTracing::OnWindowActivated(ActivationReason reason,
                                                 aura::Window* gained_active,
                                                 aura::Window* lost_active) {
  // Discard any active tracing if any.
  MaybeStopTracing();

  // Stop and report previous active window's jankiness tracing so far.
  FinalizeJankinessTracing(true /* stopped_early */);

  // Detach previous active window if it is set.
  DetachActiveWindow();

  // Ignore any non-ARC++ window.
  const auto maybe_task_id = arc::GetWindowTaskId(gained_active);
  if (!maybe_task_id.has_value()) {
    return;
  }

  // Ghost window is not an actual app window.
  if (gained_active->GetProperty(ash::full_restore::kArcGhostSurface))
    return;

  exo::Surface* const surface = exo::GetShellRootSurface(gained_active);
  if (!surface) {
    return;
  }

  // Observe active ARC++ window.
  AttachActiveWindow(gained_active, surface);

  StartJankinessTracing();

  MaybeStartTracing();
}

void ArcAppPerformanceTracing::OnWindowDestroying(aura::Window* window) {
  // ARC++ window will be destroyed.
  DCHECK_EQ(arc_active_window_, window);

  CancelJankinessTracing();

  MaybeStopTracing();

  DetachActiveWindow();
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
  auto task_id = arc::GetWindowTaskId(arc_active_window_);
  DCHECK(task_id);

  const std::string& app_id = task_id_to_app_id_[*task_id].first;
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
  DCHECK(!scoped_surface_ || (scoped_surface_->get() == surface));
  scoped_surface_.reset();
}

void ArcAppPerformanceTracing::CancelJankinessTracing() {
  jankiness_timer_.Stop();
}

void ArcAppPerformanceTracing::FinalizeJankinessTracing(bool stopped_early) {
  // Never started. Nothing to do.
  if (!jankiness_timer_.IsRunning() && stopped_early)
    return;

  jankiness_timer_.Stop();

  // Check if we have all conditions met, ARC++ window is active and information
  // is available for associated task.
  if (!arc_active_window_)
    return;

  auto task_id = arc::GetWindowTaskId(arc_active_window_);
  DCHECK(task_id);

  const auto it = task_id_to_app_id_.find(*task_id);
  if (it == task_id_to_app_id_.end())
    // It is normal that information might not be available at this time.
    return;

  // Test instances might not have Service Manager running.
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->metrics(), GetGfxMetrics);
  if (!instance)
    return;

  const std::string package_name = it->second.second;
  auto callback = base::BindOnce(&ArcAppPerformanceTracing::OnGfxMetrics,
                                 base::Unretained(this), package_name);
  instance->GetGfxMetrics(package_name, std::move(callback));

  // Finalized normally, safe to restart.
  if (!stopped_early)
    StartJankinessTracing();
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

bool ArcAppPerformanceTracing::WasReported(const std::string& category) const {
  DCHECK(!category.empty());
  return reported_categories_.count(category);
}

void ArcAppPerformanceTracing::SetReported(const std::string& category) {
  DCHECK(!category.empty());
  reported_categories_.insert(category);
}

void ArcAppPerformanceTracing::MaybeStartTracing() {
  if (session_) {
    // We are already tracing, ignore.
    DCHECK_EQ(session_->window(), arc_active_window_);
    return;
  }

  // Check if we have all conditions met, ARC++ window is active and information
  // is available for associated task.
  if (!arc_active_window_)
    return;

  auto task_id = arc::GetWindowTaskId(arc_active_window_);
  DCHECK(task_id);

  const auto it = task_id_to_app_id_.find(*task_id);
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
    // TODO(crbug.com/1330894): This should probably handled by
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

  // Start tracing for |arc_active_window_|.
  session_ =
      std::make_unique<ArcAppPerformanceTracingUmaSession>(this, category);
  session_->Schedule();
}

void ArcAppPerformanceTracing::MaybeStopTracing() {
  // Reset tracing if it was set.
  session_.reset();
}

void ArcAppPerformanceTracing::AttachActiveWindow(aura::Window* window,
                                                  exo::Surface* surface) {
  DCHECK(window);
  DCHECK(!arc_active_window_);
  arc_active_window_ = window;
  arc_active_window_->AddObserver(this);

  // Use scoped surface observer to be safe on the surface
  // destruction. |exo::GetShellRootSurface| would fail in case
  // the surface gets destroyed before widget.
  scoped_surface_ =
      std::make_unique<exo::ScopedSurface>(surface, this /* observer */);
}

void ArcAppPerformanceTracing::DetachActiveWindow() {
  if (!arc_active_window_)
    return;

  scoped_surface_.reset();
  arc_active_window_->RemoveObserver(this);
  arc_active_window_ = nullptr;
}

// static
void ArcAppPerformanceTracing::EnsureFactoryBuilt() {
  ArcAppPerformanceTracingFactory::GetInstance();
}

}  // namespace arc
