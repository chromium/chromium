// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_custom_session.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_uma_session.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"

namespace arc {

namespace {

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
  }
  ~ArcAppPerformanceTracingFactory() override = default;
};

// Singleton for app id to category mapping.
class AppToCategoryMapper {
 public:
  AppToCategoryMapper() {
    // Please refer to
    // https://goto.google.com/arc++app-runtime-performance-metrics.
    Add("iicceeckdelepgbcpojbgahbhnklpane", "OnlineGame");
    Add("kmglgjicdcmjphkoojighlhjejkiefih", "CasualGame");
    Add("niajncocfieigpbiamllekeadpgbhkke", "ShooterGame");
    Add("icloenboalgjkknjdficgpgpcedmmojn", "Video");
  }

  static AppToCategoryMapper& GetInstance() {
    static base::NoDestructor<AppToCategoryMapper> instance;
    return *instance.get();
  }

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

  DISALLOW_COPY_AND_ASSIGN(AppToCategoryMapper);
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
ArcAppPerformanceTracing::~ArcAppPerformanceTracing() = default;

// static
ArcAppPerformanceTracing* ArcAppPerformanceTracing::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAppPerformanceTracingFactory::GetForBrowserContext(context);
}

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
  if (!session_ || !session_->AsCustomSession())
    std::move(result_callback).Run(false /* success */, 0, 0, 0);

  session_->AsCustomSession()->StopAndAnalyze(std::move(result_callback));
}

void ArcAppPerformanceTracing::OnWindowActivated(ActivationReason reason,
                                                 aura::Window* gained_active,
                                                 aura::Window* lost_active) {
  // Discard any active tracing if any.
  MaybeStopTracing();

  // Detach previous active window if it is set.
  DetachActiveWindow();

  // Ignore any non-ARC++ window.
  if (arc::GetWindowTaskId(gained_active) <= 0)
    return;

  // Observe active ARC++ window.
  AttachActiveWindow(gained_active);

  MaybeStartTracing();
}

void ArcAppPerformanceTracing::OnWindowDestroying(aura::Window* window) {
  // ARC++ window will be destroyed.
  DCHECK_EQ(arc_active_window_, window);

  MaybeStopTracing();

  DetachActiveWindow();
}

void ArcAppPerformanceTracing::OnTaskCreated(int32_t task_id,
                                             const std::string& package_name,
                                             const std::string& activity,
                                             const std::string& intent) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  task_id_to_app_id_[task_id] = app_id;
  MaybeStartTracing();
}

void ArcAppPerformanceTracing::OnTaskDestroyed(int32_t task_id) {
  task_id_to_app_id_.erase(task_id);
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

  const int task_id = arc::GetWindowTaskId(arc_active_window_);
  DCHECK_GT(task_id, 0);

  const auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end()) {
    // It is normal that information might not be available at this time.
    return;
  }

  const std::string& category =
      AppToCategoryMapper::GetInstance().GetCategory(it->second /* app_id */);

  if (category.empty()) {
    // App is not recognized as app for tracing, ignore it.
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

void ArcAppPerformanceTracing::AttachActiveWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(!arc_active_window_);

  arc_active_window_ = window;
  arc_active_window_->AddObserver(this);
}

void ArcAppPerformanceTracing::DetachActiveWindow() {
  if (!arc_active_window_)
    return;

  arc_active_window_->RemoveObserver(this);
  arc_active_window_ = nullptr;
}

}  // namespace arc
