// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace arc::input_overlay {
namespace {

// Singleton factory for ArcInputOverlayManager.
class ArcInputOverlayManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputOverlayManager,
          ArcInputOverlayManagerFactory> {
 public:
  static constexpr const char* kName = "ArcInputOverlayManagerFactory";

  static ArcInputOverlayManagerFactory* GetInstance() {
    return base::Singleton<ArcInputOverlayManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcInputOverlayManagerFactory>;
  ArcInputOverlayManagerFactory() = default;
  ~ArcInputOverlayManagerFactory() override = default;
};

// Check if the window is still loading as a ghost window.
bool IsGhostWindowLoading(aura::Window* window) {
  DCHECK(window);
  if (!window->GetProperty(app_restore::kRealArcTaskWindow)) {
    return true;
  }
  // TODO(b/258308970): This is a workaround.
  // `GetProperty(app_restore::kRealArcTaskWindow)` doesn't give an expected
  // value. So check if the window is still loading as a ghost window by
  // checking if there is an overlay.
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  return shell_surface_base && shell_surface_base->HasOverlay();
}

void CheckWriteResult(std::string package_name, bool result) {
  if (result) {
    return;
  }
  LOG(ERROR) << "Failed to write proto for " << package_name;
}

// Returns the anchor window where `window` is anchored to if `window` is
// `BubbleDialogDelegateView` related and `window` has
// `ash::GameDashboardMainMenuView` or `ash::AnchoredNudge` as its contents
// view. Otherwise, returns nullptr.
aura::Window* GetGameBubbleDialogAnchorWindow(aura::Window* window) {
  DCHECK(window);

  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget) {
    return nullptr;
  }

  // Check whether `window` has `BubbleDialogDelegateView` or its sub-class
  // instance as its contents view.
  auto* window_delegate = widget->widget_delegate();
  if (!window_delegate) {
    return nullptr;
  }
  auto* bubble_delegate = window_delegate->AsBubbleDialogDelegate();
  if (!bubble_delegate) {
    return nullptr;
  }

  views::Widget* anchor_widget = nullptr;
  if (const auto* contents_view = bubble_delegate->GetContentsView();
      views::AsViewClass<ash::GameDashboardMainMenuView>(contents_view)) {
    // `window` has `ash::GameDashboardMainMenuView` as contents view.
    anchor_widget = widget->parent();
    DCHECK(anchor_widget);
  } else if (views::AsViewClass<ash::AnchoredNudge>(contents_view) ||
             views::AsViewClass<DeleteEditShortcut>(contents_view)) {
    // `window` has `ash::AnchoredNudge` or `DeleteEditShortcut` as contents
    // view.
    if (auto* nudge_anchor_view = bubble_delegate->GetAnchorView()) {
      anchor_widget = nudge_anchor_view->GetWidget();
      DCHECK(anchor_widget);
    }
  }

  return anchor_widget ? anchor_widget->GetNativeWindow() : nullptr;
}

}  // namespace

class ArcInputOverlayManager::InputMethodObserver
    : public ui::InputMethodObserver {
 public:
  explicit InputMethodObserver(ArcInputOverlayManager* owner) : owner_(owner) {}
  InputMethodObserver(const InputMethodObserver&) = delete;
  InputMethodObserver& operator=(const InputMethodObserver&) = delete;
  ~InputMethodObserver() override = default;

  // ui::InputMethodObserver overrides:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    owner_->is_text_input_active_ =
        client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
        client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NULL;
    owner_->NotifyTextInputState();
  }
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {
    owner_->input_method_ = nullptr;
  }

 private:
  raw_ptr<ArcInputOverlayManager> const owner_;
};

// static
ArcInputOverlayManager* ArcInputOverlayManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputOverlayManagerFactory::GetForBrowserContext(context);
}

ArcInputOverlayManager::ArcInputOverlayManager(
    content::BrowserContext* browser_context,
    ::arc::ArcBridgeService* arc_bridge_service)
    : input_method_observer_(std::make_unique<InputMethodObserver>(this)) {
  if (aura::Env::HasInstance()) {
    env_observation_.Observe(aura::Env::GetInstance());
  }
  if (ash::Shell::HasInstance() && ash::Shell::GetPrimaryRootWindow()) {
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       // Should not block shutdown.
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // For test. The unittest is based on ExoTestBase which must run on
  // Chrome_UIThread. While TestingProfileManager::CreateTestingProfile runs on
  // MainThread.
  if (browser_context) {
    data_controller_ =
        std::make_unique<DataController>(*browser_context, task_runner_);
  }
}

ArcInputOverlayManager::~ArcInputOverlayManager() = default;

// static
void ArcInputOverlayManager::EnsureFactoryBuilt() {
  ArcInputOverlayManagerFactory::GetInstance();
}

void ArcInputOverlayManager::OnWindowInitialized(aura::Window* new_window) {
  // `chromeos::kAppTypeKey` property is set in
  // `AppServiceAppWindowShelfController::OnWindowInitialized()`.
  // `AppServiceAppWindowShelfController::OnWindowInitialized()` is called
  // before `ArcInputOverlayManager::OnWindowInitialized()`, so we can filter
  // non-ARC apps here.
  if (!new_window || !ash::IsArcWindow(new_window) ||
      new_window != new_window->GetToplevelWindow() ||
      window_observations_.IsObservingSource(new_window)) {
    return;
  }

  window_observations_.AddObservation(new_window);
}

void ArcInputOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  // There are two cases when launching an app.
  // 1) Launch from Launcher: Receive {ash::kArcPackageNameKey, package_name}.
  // 2) Restore the app: Receive {ash::kArcPackageNameKey, package_name} and
  // {app_restore::kRealArcTaskWindow, true}. When `ash::kArcPackageNameKey` is
  // changed, the ghost window overlay is not destroyed. The ghost window
  // overlay is destroyed right before property
  // {app_restore::kRealArcTaskWindow} is set.
  if (!window || (key != ash::kArcPackageNameKey &&
                  key != app_restore::kRealArcTaskWindow)) {
    return;
  }

  DCHECK_EQ(window, window->GetToplevelWindow());
  if (input_overlay_enabled_windows_.contains(window) ||
      IsGhostWindowLoading(window) || loading_data_windows_.contains(window)) {
    return;
  }
  const std::string* package_name =
      window->GetProperty(ash::kArcPackageNameKey);
  if (!package_name || package_name->empty()) {
    return;
  }

  // Start to read data.
  auto touch_injector = std::make_unique<TouchInjector>(
      window, *package_name,
      base::BindRepeating(&ArcInputOverlayManager::OnSaveProtoFile,
                          weak_ptr_factory_.GetWeakPtr()));
  loading_data_windows_.insert(window);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcInputOverlayManager::ReadDefaultData,
                     std::move(touch_injector)),
      base::BindOnce(&ArcInputOverlayManager::OnFinishReadDefaultData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcInputOverlayManager::OnWindowDestroying(aura::Window* window) {
  UnregisterAndRemoveObservation(window);
}

void ArcInputOverlayManager::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window ||
      ash::window_util::GetFocusedWindow()->GetToplevelWindow() != window) {
    return;
  }
  RegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (!window) {
    return;
  }
  // There might be child window surface removing, we don't unregister window
  // until the top_level_window is removed from the root.
  UnRegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowParentChanged(aura::Window* window,
                                                   aura::Window* parent) {
  // Ignore if `parent` is a container.
  if (!parent || parent != parent->GetToplevelWindow()) {
    return;
  }
  UnregisterAndRemoveObservation(window);
}

void ArcInputOverlayManager::Shutdown() {
  UnRegisterWindow(registered_top_level_window_);
  window_observations_.RemoveAllObservations();
  if (ash::Shell::HasInstance() && ash::Shell::GetPrimaryRootWindow()) {
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  }
  if (aura::Env::HasInstance()) {
    env_observation_.Reset();
  }
}

void ArcInputOverlayManager::OnWindowFocused(aura::Window* gained_focus,
                                             aura::Window* lost_focus) {
  // No need to register window if it is tablet mode and there is no game
  // window.
  if (display::Screen::GetScreen()->InTabletMode() ||
      input_overlay_enabled_windows_.empty()) {
    return;
  }

  aura::Window* lost_focus_top_level_window = nullptr;
  aura::Window* gained_focus_top_level_window = nullptr;

  if (lost_focus) {
    lost_focus_top_level_window = lost_focus->GetToplevelWindow();
  }

  if (gained_focus) {
    gained_focus_top_level_window = gained_focus->GetToplevelWindow();
  }

  auto* gained_anchor_window = GetAnchorWindow(gained_focus_top_level_window);
  auto* lost_anchor_window = GetAnchorWindow(lost_focus_top_level_window);

  if (gained_anchor_window == lost_anchor_window) {
    return;
  }

  UnRegisterWindow(lost_anchor_window);
  RegisterWindow(gained_anchor_window);
}

void ArcInputOverlayManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!registered_top_level_window_) {
    return;
  }

  if (auto it =
          input_overlay_enabled_windows_.find(registered_top_level_window_);
      it != input_overlay_enabled_windows_.end()) {
    it->second->UpdatePositionsForRegister();
  }
}

void ArcInputOverlayManager::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
      RegisterFocusedWindow();
      break;
    case display::TabletState::kEnteringTabletMode:
      UnRegisterWindow(registered_top_level_window_);
      break;
    case display::TabletState::kInTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void ArcInputOverlayManager::RemoveWindowObservation(aura::Window* window) {
  if (window_observations_.IsObservingSource(window)) {
    window_observations_.RemoveObservation(window);
  }
}

void ArcInputOverlayManager::UnregisterAndRemoveObservation(
    aura::Window* window) {
  UnRegisterWindow(window);
  input_overlay_enabled_windows_.erase(window);
  loading_data_windows_.erase(window);
  RemoveWindowObservation(window);
}

// static
std::unique_ptr<TouchInjector> ArcInputOverlayManager::ReadDefaultData(
    std::unique_ptr<TouchInjector> touch_injector) {
  DCHECK(touch_injector);

  const std::string& package_name = touch_injector->package_name();
  const auto resource_id = GetInputOverlayResourceId(package_name);
  if (!resource_id) {
    return touch_injector;
  }

  std::string json_file =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          resource_id.value());
  if (json_file.empty()) {
    LOG(WARNING) << "No content for: " << package_name;
    return touch_injector;
  }
  const auto result = base::JSONReader::ReadAndReturnValueWithError(json_file);
  DCHECK(result.has_value())
      << "Could not load input overlay data file: " << result.error().message;
  if (!result.has_value() || !result->is_dict()) {
    return touch_injector;
  }

  touch_injector->ParseActions(result->GetDict());
  return touch_injector;
}

void ArcInputOverlayManager::OnFinishReadDefaultData(
    std::unique_ptr<TouchInjector> touch_injector) {
  DCHECK(touch_injector);

  if (!IsBeta() && touch_injector->actions().empty()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  // Null for unit test.
  if (!data_controller_) {
    OnProtoDataAvailable(std::move(touch_injector), /*proto=*/nullptr);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DataController::ReadProtoFromFile,
                     data_controller_->GetFilePathFromPackageName(
                         touch_injector->package_name())),
      base::BindOnce(&ArcInputOverlayManager::OnProtoDataAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(touch_injector)));
}

void ArcInputOverlayManager::OnProtoDataAvailable(
    std::unique_ptr<TouchInjector> touch_injector,
    std::unique_ptr<AppDataProto> proto) {
  DCHECK(touch_injector);
  if (proto) {
    touch_injector->OnProtoDataAvailable(*proto);
  } else {
    touch_injector->NotifyFirstTimeLaunch();
  }

  if (!IsBeta()) {
    DCHECK(!touch_injector->actions().empty());
    OnLoadingFinished(std::move(touch_injector));
    return;
  }

  // Steps to check whether enablings Game Controls for `package_name`.
  // 1) Check whether the app opts out Game Controls explicitly.
  // 2) Check whether the app is a game.
  // 3) Check whether the app is an Optimized-for-ChromeOS app.

  // If Game Controls is opt-out explicitly, Game Controls is not available for
  // this app.
  if (IsGameControlsOptOut(touch_injector->package_name())) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  CheckAppCategory(std::move(touch_injector));
}

void ArcInputOverlayManager::OnSaveProtoFile(
    std::unique_ptr<AppDataProto> proto,
    std::string package_name) {
  if (!data_controller_) {
    return;
  }
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &DataController::WriteProtoToFile, std::move(proto),
          data_controller_->GetFilePathFromPackageName(package_name)),
      base::BindOnce(&CheckWriteResult, package_name));
}

bool ArcInputOverlayManager::IsGameControlsOptOut(
    const std::string& package_name) {
  auto* prefs = GetArcAppListPrefs();
  CHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(package_name);
  return package && package->game_controls_opt_out;
}

void ArcInputOverlayManager::CheckAppCategory(
    std::unique_ptr<TouchInjector> touch_injector) {
  auto* prefs = GetArcAppListPrefs();
  CHECK(prefs);
  const std::string package_name = touch_injector->package_name();
  const auto app_category =
      prefs->GetAppCategory(prefs->GetAppIdByPackageName(package_name));
  // If the app is not a game, Game Controls is not available for this app.
  if (app_category != arc::mojom::AppCategory::kUndefined &&
      app_category != arc::mojom::AppCategory::kGame) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  if (app_category == arc::mojom::AppCategory::kGame) {
    // Check if it is an O4C game.
    CheckO4C(std::move(touch_injector));
  } else {
    // It is possible that `app_category` is not cached yet. If `app_category`
    // is not cached, it calls mojom function explicitly to fetch `app_category`
    // from Android side.
    auto* connection = prefs->app_connection_holder();
    if (!connection) {
      LOG(ERROR)
          << "Unable to get access to GetAppCategory for nullptr |connection|.";
      MayKeepTouchInjectorAfterError(std::move(touch_injector));
      return;
    }

    auto* instance = ARC_GET_INSTANCE_FOR_METHOD(connection, GetAppCategory);
    if (!instance) {
      LOG(ERROR) << "GetAppCategory method for ARC is not available";
      MayKeepTouchInjectorAfterError(std::move(touch_injector));
      return;
    }

    instance->GetAppCategory(
        package_name,
        base::BindOnce(&ArcInputOverlayManager::OnDidCheckAppCategory,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(touch_injector)));
  }
}

void ArcInputOverlayManager::OnDidCheckAppCategory(
    std::unique_ptr<TouchInjector> touch_injector,
    arc::mojom::AppCategory app_category) {
  // If the app is not a game, Game Controls is not available for this app.
  if (app_category != arc::mojom::AppCategory::kGame) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }
  // Check whether it is an Optimized-for-ChromeOS games.
  CheckO4C(std::move(touch_injector));
}

void ArcInputOverlayManager::CheckO4C(
    std::unique_ptr<TouchInjector> touch_injector) {
  // Check if it is an O4C app from mojom instance.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(ERROR) << "Failed to get ArcServiceManager";
    OnLoadingFinished(std::move(touch_injector));
    return;
  }
  auto* compatibility_mode =
      arc_service_manager->arc_bridge_service()->compatibility_mode();
  if (!compatibility_mode || !compatibility_mode->IsConnected()) {
    // This mojom is available for R and newer.
    LOG(ERROR) << "No supported Android connection for compatibility_mode.";
    OnLoadingFinished(std::move(touch_injector));
    return;
  }
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(compatibility_mode, IsOptimizedForCrosApp);
  if (!instance) {
    LOG(ERROR) << "IsOptimizedForCrosApp method for ARC is not available.";
    OnLoadingFinished(std::move(touch_injector));
    return;
  }

  const std::string package_name = touch_injector->package_name();
  VLOG(2) << "Check if pkg: " << package_name << " is an O4C app.";

  instance->IsOptimizedForCrosApp(
      package_name, base::BindOnce(&ArcInputOverlayManager::OnLoadingFinished,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(touch_injector)));
}

void ArcInputOverlayManager::NotifyTextInputState() {
  if (const auto it =
          input_overlay_enabled_windows_.find(registered_top_level_window_);
      it != input_overlay_enabled_windows_.end()) {
    it->second->NotifyTextInputState(is_text_input_active_);
  }
}

void ArcInputOverlayManager::AddObserverToInputMethod() {
  if (!registered_top_level_window_) {
    return;
  }
  DCHECK(registered_top_level_window_->GetHost());
  DCHECK(!input_method_);
  input_method_ = registered_top_level_window_->GetHost()->GetInputMethod();
  if (input_method_) {
    input_method_->AddObserver(input_method_observer_.get());
  }
}

void ArcInputOverlayManager::RemoveObserverFromInputMethod() {
  if (!input_method_) {
    return;
  }
  input_method_->RemoveObserver(input_method_observer_.get());
  input_method_ = nullptr;
}

void ArcInputOverlayManager::RegisterWindow(aura::Window* window) {
  // Only register the focused window that is not registered.
  if (!window || window != window->GetToplevelWindow() ||
      registered_top_level_window_ == window) {
    return;
  }

  // It should always unregister the window first, then register another window.
  DCHECK(!registered_top_level_window_);

  // For Beta version, it may focus on its transient sibling window.
  if (!IsBeta()) {
    DCHECK_EQ(ash::window_util::GetFocusedWindow()->GetToplevelWindow(),
              window);
  }

  const auto it = input_overlay_enabled_windows_.find(window);
  if (it == input_overlay_enabled_windows_.end()) {
    return;
  }
  it->second->RegisterEventRewriter();
  registered_top_level_window_ = window;
  AddObserverToInputMethod();
  AddDisplayOverlayController(it->second.get());
  // If the window is on the extended window, it turns out only primary root
  // window catches the key event. So it needs to forward the key event from
  // primary root window to extended root window event source.
  if (registered_top_level_window_->GetRootWindow() !=
      ash::Shell::GetPrimaryRootWindow()) {
    key_event_source_rewriter_ =
        std::make_unique<KeyEventSourceRewriter>(registered_top_level_window_);
  }
}

void ArcInputOverlayManager::UnRegisterWindow(aura::Window* window) {
  if (!registered_top_level_window_ || registered_top_level_window_ != window) {
    return;
  }
  const auto it =
      input_overlay_enabled_windows_.find(registered_top_level_window_);
  DCHECK(it != input_overlay_enabled_windows_.end());
  if (it == input_overlay_enabled_windows_.end()) {
    return;
  }
  if (key_event_source_rewriter_) {
    key_event_source_rewriter_.reset();
  }
  it->second->UnRegisterEventRewriter();
  RemoveDisplayOverlayController();
  RemoveObserverFromInputMethod();
  it->second->NotifyTextInputState(false);
  registered_top_level_window_ = nullptr;
}

void ArcInputOverlayManager::RegisterFocusedWindow() {
  // Register window if it is not in tablet mode.
  if (auto* focused_window = ash::window_util::GetFocusedWindow();
      focused_window && !display::Screen::GetScreen()->InTabletMode()) {
    RegisterWindow(GetAnchorWindow(focused_window->GetToplevelWindow()));
  }
}

void ArcInputOverlayManager::AddDisplayOverlayController(
    TouchInjector* touch_injector) {
  DCHECK(registered_top_level_window_);
  DCHECK(touch_injector);
  if (!registered_top_level_window_ || !touch_injector) {
    return;
  }
  DCHECK(!display_overlay_controller_);

  display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
      touch_injector, touch_injector->first_launch());
}

void ArcInputOverlayManager::RemoveDisplayOverlayController() {
  if (!registered_top_level_window_) {
    return;
  }

  // There is only one `display_overlay_controller_` active at a time. When
  // window is destroyed, the attached sibling window is destroyed first, which
  // triggers the window focus change. And then it also triggers the window
  // unregister and gets `display_overlay_controller_` reset before here.
  if (!IsBeta()) {
    DCHECK(display_overlay_controller_);
  }

  if (display_overlay_controller_) {
    display_overlay_controller_.reset();
  }
}

void ArcInputOverlayManager::ResetForPendingTouchInjector(
    std::unique_ptr<TouchInjector> touch_injector) {
  auto* window = touch_injector->window();

  // If `window` is destroyed, it will be removed from `loading_data_window_` by
  // OnWindowDestroying(). So it is safe to call Window class functions after
  // checking `loading_data_window_`.
  if (IsBeta() && loading_data_windows_.contains(window) &&
      !window->is_destroying()) {
    // GIO status is known here and GIO is not available.
    window->SetProperty(ash::kArcGameControlsFlagsKey,
                        ash::ArcGameControlsFlag::kKnown);
  }
  loading_data_windows_.erase(window);
  touch_injector.reset();
  RemoveWindowObservation(window);
}

void ArcInputOverlayManager::OnLoadingFinished(
    std::unique_ptr<TouchInjector> touch_injector,
    bool is_o4c) {
  auto* window = touch_injector->window();
  DCHECK(window);
  // Check if `window` is destroyed or destroying when calling this function.
  if (!loading_data_windows_.contains(window) || window->is_destroying()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  touch_injector->UpdateFlags(is_o4c);

  // Record the menu state when there is at least one action.
  if (!touch_injector->actions().empty()) {
    touch_injector->RecordMenuStateOnLaunch();
  }

  input_overlay_enabled_windows_.emplace(window, std::move(touch_injector));
  loading_data_windows_.erase(window);
  RegisterFocusedWindow();
}

void ArcInputOverlayManager::MayKeepTouchInjectorAfterError(
    std::unique_ptr<TouchInjector> touch_injector) {
  if (touch_injector->actions().empty()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
  } else {
    OnLoadingFinished(std::move(touch_injector));
  }
}

ArcAppListPrefs* ArcInputOverlayManager::GetArcAppListPrefs() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(arc::IsArcAllowedForProfile(profile));
  return ArcAppListPrefs::Get(profile);
}

aura::Window* ArcInputOverlayManager::GetAnchorWindow(aura::Window* window) {
  // TODO(b/314687082): It still needs to find a way to reproduce the crash.
  // Right now, return `window` directly for pre-beta version to stabilize
  // ChromeOS.
  if (!IsBeta() || !window) {
    return window;
  }

  // Check whether `window` is a bubble dialog window.
  auto* bubble_anchor_window = GetGameBubbleDialogAnchorWindow(window);
  auto* pending_window = bubble_anchor_window ? bubble_anchor_window : window;

  // There are two types of the transient sibling window:
  // - window A: the transient sibling window of the main game window, such as
  // input mapping.
  // - window B: the transient sibling window of the window A, such as editing
  // list and button options menu.

  // Check whether `pending_window` is a transient sibling window (window A or
  // window B) if `transient_parent` is not nullptr.
  auto* transient_parent = wm::GetTransientParent(pending_window);

  if (transient_parent) {
    // `pending_window` is window A or B. Check whether `pending_window` is the
    // transient window (window B) of window A (such as input mapping) if
    // `transient_parent_parent` is not nullptr.
    if (auto* transient_parent_parent =
            wm::GetTransientParent(transient_parent)) {
      // `pending_window` is window B.
      return transient_parent_parent;
    }
    return transient_parent;
  }
  return window;
}

}  // namespace arc::input_overlay
