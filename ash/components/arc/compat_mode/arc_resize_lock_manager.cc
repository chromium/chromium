// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/arc_resize_lock_manager.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/compat_mode/arc_splash_screen_dialog_view.h"
#include "ash/components/arc/compat_mode/arc_window_property_util.h"
#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"
#include "ash/components/arc/compat_mode/metrics.h"
#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/compat_mode/touch_mode_mouse_rewriter.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/resize_shadow_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

namespace {

// Singleton factory for ArcResizeLockManager.
class ArcResizeLockManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcResizeLockManager,
          ArcResizeLockManagerFactory> {
 public:
  static constexpr const char* kName = "ArcResizeLockManagerFactory";

  static ArcResizeLockManagerFactory* GetInstance() {
    return base::Singleton<ArcResizeLockManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcResizeLockManagerFactory>;
  ArcResizeLockManagerFactory() = default;
  ~ArcResizeLockManagerFactory() override = default;
};

// A self-deleting window activation observer that runs the given callback when
// its associated window gets activated.
class WindowActivationObserver : public wm::ActivationChangeObserver,
                                 public aura::WindowObserver {
 public:
  WindowActivationObserver(const WindowActivationObserver&) = delete;
  WindowActivationObserver& operator=(const WindowActivationObserver&) = delete;

  static void RunOnActivated(aura::Window* window,
                             base::OnceClosure on_activated) {
    // ash::Shell can be null in unittests.
    if (!ash::Shell::HasInstance())
      return;

    if (ash::Shell::Get()->activation_client()->GetActiveWindow() == window) {
      std::move(on_activated).Run();
      return;
    }

    // The following instance self-destructs when the window gets activated or
    // destroyed before getting activated.
    new WindowActivationObserver(window, std::move(on_activated));
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(observer_.IsObservingSource(window));
    delete this;
  }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (gained_active != window_)
      return;
    RemoveAllObservers();
    // To avoid nested-activation, here we post the task to the queue.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_activated_));
    delete this;
  }

 private:
  WindowActivationObserver(aura::Window* window, base::OnceClosure on_activated)
      : window_(window), on_activated_(std::move(on_activated)) {
    DCHECK(!on_activated_.is_null());
    ash::Shell::Get()->activation_client()->AddObserver(this);
    observer_.Observe(window_.get());
  }

  ~WindowActivationObserver() override { RemoveAllObservers(); }

  void RemoveAllObservers() {
    observer_.Reset();
    ash::Shell::Get()->activation_client()->RemoveObserver(this);
  }

  const raw_ptr<aura::Window> window_;
  base::OnceClosure on_activated_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

// A self-deleting window property observer that runs the given callback when
// its ash::kAppIDKey is set to non-null value.
class AppIdObserver : public aura::WindowObserver {
 public:
  AppIdObserver(const AppIdObserver&) = delete;
  AppIdObserver& operator=(const AppIdObserver&) = delete;

  static void RunOnReady(aura::Window* window,
                         base::OnceCallback<void(aura::Window*)> on_ready) {
    if (GetAppId(window)) {
      std::move(on_ready).Run(window);
      return;
    }

    // The following instance self-destructs when the window gets activated or
    // destroyed before getting activated.
    new AppIdObserver(window, std::move(on_ready));
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(observer_.IsObservingSource(window));
    delete this;
  }
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK(observer_.IsObservingSource(window));
    if (key != ash::kAppIDKey)
      return;
    if (!GetAppId(window))
      return;
    observer_.Reset();
    std::move(on_ready_).Run(window);
    delete this;
  }

 private:
  AppIdObserver(aura::Window* window,
                base::OnceCallback<void(aura::Window*)> on_ready)
      : window_(window), on_ready_(std::move(on_ready)) {
    DCHECK(!on_ready_.is_null());
    observer_.Observe(window_.get());
  }

  ~AppIdObserver() override { observer_.Reset(); }

  const raw_ptr<aura::Window> window_;
  base::OnceCallback<void(aura::Window*)> on_ready_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

bool ShouldEnableResizeLock(ash::ArcResizeLockType type) {
  return type != ash::ArcResizeLockType::NONE &&
         type != ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE;
}

}  // namespace

// static
ArcResizeLockManager* ArcResizeLockManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcResizeLockManagerFactory::GetForBrowserContext(context);
}

ArcResizeLockManager::ArcResizeLockManager(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : compat_mode_button_controller_(
          std::make_unique<CompatModeButtonController>()),
      touch_mode_mouse_rewriter_(std::make_unique<TouchModeMouseRewriter>()) {
  if (aura::Env::HasInstance())
    env_observation.Observe(aura::Env::GetInstance());
}

ArcResizeLockManager::~ArcResizeLockManager() = default;

void ArcResizeLockManager::OnWindowInitialized(aura::Window* new_window) {
  if (!ash::IsArcWindow(new_window))
    return;

  if (window_observations_.IsObservingSource(new_window))
    return;

  window_observations_.AddObservation(new_window);

  AppIdObserver::RunOnReady(
      new_window,
      base::BindOnce(
          [](base::WeakPtr<ArcResizeLockManager> manager,
             aura::Window* window) {
            if (!manager)
              return;
            if (!manager->pref_delegate_)
              return;
            const auto state =
                manager->pref_delegate_->GetResizeLockState(*GetAppId(window));
            RecordResizeLockStateHistogram(
                ResizeLockStateHistogramType::InitialState, state);
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcResizeLockManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const void* key,
                                                   intptr_t old) {
  // TODO(b/344640055): Replace below simple fix.
  // The overlay layer might be added before `chromeos::kIsGameKey` is set for
  // splash screen dialog. Once `chromeos::kIsGameKey` is set to true, remove
  // the overlay layer.
  if (key == chromeos::kIsGameKey &&
      window->GetProperty(chromeos::kIsGameKey)) {
    OverlayDialog::CloseIfAny(window);
    return;
  }

  if (key != ash::kArcResizeLockTypeKey)
    return;

  const auto new_value = window->GetProperty(ash::kArcResizeLockTypeKey);
  const auto old_value = static_cast<ash::ArcResizeLockType>(old);

  if (new_value != old_value) {
    AppIdObserver::RunOnReady(
        window, base::BindOnce(
                    [](base::WeakPtr<ArcResizeLockManager> manager,
                       aura::Window* window) {
                      if (!manager)
                        return;
                      if (ShouldEnableResizeLock(window->GetProperty(
                              ash::kArcResizeLockTypeKey))) {
                        manager->EnableResizeLock(window);
                      } else {
                        manager->DisableResizeLock(window);
                      }
                      // EnableResizeLock() and DisableResizeLock() are supposed
                      // to be called only when resizability is toggled while
                      // resize lock state may need to be updated even when
                      // resizability doesn't change (e.g.NONE ->
                      // RESIZE_ENABLED_TOGGLABLE)
                      manager->UpdateResizeLockState(window);
                    },
                    weak_ptr_factory_.GetWeakPtr()));
  }

  // We need to always trigger UpdateCompatModeButton regardless of value
  // change because it need to be called even when the property is set to
  // ArcResizeLockType::NONE, which is the the default value of
  // kArcResizeLockTypeKey, and the new value is the same as |old| in that case.
  AppIdObserver::RunOnReady(
      window, base::BindOnce(&CompatModeButtonController::Update,
                             compat_mode_button_controller_->GetWeakPtr()));
}

void ArcResizeLockManager::Shutdown() {
  compat_mode_button_controller_->ClearPrefDelegate();
  pref_delegate_ = nullptr;
}

void ArcResizeLockManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  compat_mode_button_controller_->Update(window);
}

void ArcResizeLockManager::OnWindowDestroying(aura::Window* window) {
  resize_lock_enabled_windows_.erase(window);
  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
}

void ArcResizeLockManager::SetPrefDelegate(
    ArcResizeLockPrefDelegate* delegate) {
  CHECK(!pref_delegate_);
  pref_delegate_ = delegate;
  compat_mode_button_controller_->SetPrefDelegate(delegate);
}

void ArcResizeLockManager::EnableResizeLock(aura::Window* window) {
  const bool inserted = resize_lock_enabled_windows_.insert(window).second;
  if (!inserted)
    return;

  // TODO(tetsui): Reconsider the trigger condition after experimenting i.e.
  // whether it is reasonable to have it enabled when ResizeLock is enabled.
  if (touch_mode_mouse_rewriter_)
    touch_mode_mouse_rewriter_->EnableForWindow(window);

  const auto app_id = GetAppId(window);
  DCHECK(app_id);
  const bool is_fully_locked =
      window->GetProperty(ash::kArcResizeLockTypeKey) ==
      ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE;

  // The state is |ArcResizeLockState::READY| only when we enable the resize
  // lock for an app for the first time. UpdateResizeLockState() may overwrite
  // the ResizeLockState so this check must be done before it's called.
  const bool is_first_launch = pref_delegate_->GetResizeLockState(*app_id) ==
                               mojom::ArcResizeLockState::READY;
  UpdateResizeLockState(window);

  // No need to show splash screen dialog for game apps.
  if (is_first_launch && ShouldShowSplashScreenDialog(pref_delegate_) &&
      !ash::GameDashboardController::IsGameWindow(window)) {
    // UpdateResizeLockState() must be called beforehand as compat-mode button
    // must exist before showing the splash dialog because it's used as the
    // anchoring target.
    ShowSplashScreenDialog(window, is_fully_locked);
  }

  if (!is_fully_locked) {
    window->SetProperty(ash::kUnresizableSnappedSizeKey,
                        new gfx::Size(GetUnresizableSnappedWidth(window), 0));
  } else {
    window->ClearProperty(ash::kUnresizableSnappedSizeKey);
  }
}

void ArcResizeLockManager::DisableResizeLock(aura::Window* window) {
  const bool erased = resize_lock_enabled_windows_.erase(window);
  if (!erased)
    return;
  window->SetProperty(ash::kResizeShadowTypeKey,
                      ash::ResizeShadowType::kUnlock);
  // Hide shadow effect on window. ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->resize_shadow_controller()->HideShadow(window);

  if (touch_mode_mouse_rewriter_)
    touch_mode_mouse_rewriter_->DisableForWindow(window);

  window->ClearProperty(ash::kUnresizableSnappedSizeKey);
}

void ArcResizeLockManager::UpdateResizeLockState(aura::Window* window) {
  const auto app_id = GetAppId(window);
  DCHECK(app_id);
  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
      pref_delegate_->SetResizeLockState(*app_id,
                                         mojom::ArcResizeLockState::ON);
      break;
    case ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
      pref_delegate_->SetResizeLockState(
          *app_id, mojom::ArcResizeLockState::FULLY_LOCKED);
      break;
    case ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
      pref_delegate_->SetResizeLockState(*app_id,
                                         mojom::ArcResizeLockState::OFF);
      break;
    case ash::ArcResizeLockType::NONE:
      // Maximizing an app with RESIZE_ENABLED_TOGGLABLE can lead to this case.
      // Resize lock state shouldn't be updated as the pre-maximized state
      // needs to be restored later.
      break;
  }

  // As we updated the resize lock state above, we need to update compat mode
  // button.
  compat_mode_button_controller_->Update(window);

  // Even if resize lock doesn't get enabled or disabled, we need to ensure to
  // update this as resize shadow can be updated in an intermediate state when
  // exo commites the next state.
  UpdateShadow(window);
}

void ArcResizeLockManager::UpdateShadow(aura::Window* window) {
  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  const bool resize_lock_enabled =
      resize_lock_type == ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE ||
      resize_lock_type == ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE;
  const auto resize_shadow_type = resize_lock_enabled
                                      ? ash::ResizeShadowType::kLock
                                      : ash::ResizeShadowType::kUnlock;
  window->SetProperty(ash::kResizeShadowTypeKey, resize_shadow_type);
  // ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance()) {
    if (resize_lock_enabled) {
      ash::Shell::Get()->resize_shadow_controller()->ShowShadow(window);
    } else {
      ash::Shell::Get()->resize_shadow_controller()->HideShadow(window);
    }
  }
}

void ArcResizeLockManager::ShowSplashScreenDialog(aura::Window* window,
                                                  bool is_fully_locked) {
  WindowActivationObserver::RunOnActivated(
      window, base::BindOnce(&ArcSplashScreenDialogView::Show, window,
                             is_fully_locked));
}

// static
void ArcResizeLockManager::EnsureFactoryBuilt() {
  ArcResizeLockManagerFactory::GetInstance();
}

}  // namespace arc
