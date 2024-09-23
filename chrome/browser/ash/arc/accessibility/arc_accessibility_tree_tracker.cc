// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_tree_tracker.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/arc_serialization_delegate.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/window_properties.h"
#include "extensions/browser/event_router.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

using SetNativeChromeVoxResponse =
    extensions::api::accessibility_private::SetNativeChromeVoxResponse;

ArcAccessibilityTreeTracker::TreeKey KeyForInputMethod() {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kInputMethod,
          kNoTaskId,
          {} /* notification_key */};
}

ArcAccessibilityTreeTracker::TreeKey KeyForNotification(
    std::string notification_key) {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kNotificationKey, kNoTaskId,
          std::move(notification_key)};
}

ArcAccessibilityTreeTracker::TreeKey KeyForTaskId(int32_t task_id) {
  return {ArcAccessibilityTreeTracker::TreeKeyType::kTaskId,
          task_id,
          {} /* notification_key */};
}

void SetChildAxTreeIDForWindow(aura::Window* window,
                               const ui::AXTreeID& treeID) {
  DCHECK(window);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget)
    return;

  static_cast<exo::ShellSurfaceBase*>(widget->widget_delegate())
      ->SetChildAxTreeId(treeID);
}

void UpdateTreeIdOfNotificationSurface(const std::string& notification_key,
                                       const ui::AXTreeID& tree_id) {
  auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
  if (!surface_manager)
    return;

  ash::ArcNotificationSurface* surface =
      surface_manager->GetArcSurface(notification_key);
  if (!surface)
    return;

  surface->SetAXTreeId(tree_id);

  if (surface->IsAttached()) {
    // Dispatch ax::mojom::Event::kChildrenChanged to force AXNodeData of the
    // notification updated.
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, false);
  }
}

std::optional<int32_t> FindAccessibilityWindowIdRecursive(
    aura::Window* window) {
  if (const std::optional<int32_t> window_id =
          exo::GetShellClientAccessibilityId(window)) {
    return window_id;
  }
  for (aura::Window* child : window->children()) {
    if (const std::optional<int32_t> window_id =
            FindAccessibilityWindowIdRecursive(child)) {
      return window_id;
    }
  }
  return std::nullopt;
}

extensions::api::accessibility_private::SetNativeChromeVoxResponse
FromMojomResponseToAutomationResponse(
    ax::android::mojom::SetNativeChromeVoxResponse response) {
  using MojomResponse = ax::android::mojom::SetNativeChromeVoxResponse;
  switch (response) {
    case MojomResponse::SUCCESS:
      return SetNativeChromeVoxResponse::kSuccess;
    case MojomResponse::TALKBACK_NOT_INSTALLED:
      return SetNativeChromeVoxResponse::kTalkbackNotInstalled;
    case MojomResponse::WINDOW_NOT_FOUND:
      return SetNativeChromeVoxResponse::kWindowNotFound;
    case MojomResponse::FAILURE:
      return SetNativeChromeVoxResponse::kFailure;
    case MojomResponse::NEED_DEPRECATION_CONFIRMATION:
      return SetNativeChromeVoxResponse::kNeedDeprecationConfirmation;
    case MojomResponse::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      return SetNativeChromeVoxResponse::kFailure;
  }
}

}  // namespace

class ArcAccessibilityTreeTracker::FocusChangeObserver
    : public aura::client::FocusChangeObserver {
 public:
  explicit FocusChangeObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->AddFocusObserver(this);
  }
  ~FocusChangeObserver() override {
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->RemoveFocusObserver(this);
  }

  void OnWindowFocused(aura::Window* original_gained_focus,
                       aura::Window* original_lost_focus) override {
    aura::Window* gained_focus = FindArcOrGhostWindow(original_gained_focus);
    aura::Window* lost_focus = FindArcOrGhostWindow(original_lost_focus);
    if (gained_focus == lost_focus)
      return;

    owner_->OnWindowFocused(gained_focus, lost_focus);
  }

 private:
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
  // Different from other inner classes, this doesn't use ScopedObservation
  // because exo::WMHelper can be destroyed earlier than this class.
};

// Observes windows corresponds to each task.
class ArcAccessibilityTreeTracker::WindowsObserver
    : public aura::WindowObserver {
 public:
  explicit WindowsObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {}

  void Observe(aura::Window* window) {
    if (window_observations_.IsObservingSource(window))
      return;

    window_observations_.AddObservation(window);
  }

  void Reset() { window_observations_.RemoveAllObservations(); }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != exo::kApplicationIdKey) {
      return;
    }
    owner_->UpdateTopWindowIds(window);
  }

  void OnWindowAdded(aura::Window* new_window) override {
    owner_->TrackChildWindow(new_window);
  }

  void OnWindowDestroying(aura::Window* window) override {
    if (window_observations_.IsObservingSource(window))
      window_observations_.RemoveObservation(window);
    owner_->OnWindowDestroying(window);
  }

  int GetTrackingWindowCount() const {
    return window_observations_.GetSourcesCount();
  }

 private:
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

// Observes child windows of toplevel ARC++ window, in order to get a value for
// |ash::kClientAccessibilityIdKey|, which is set on window level.
class ArcAccessibilityTreeTracker::ChildWindowsObserver
    : public aura::WindowObserver {
 public:
  explicit ChildWindowsObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {}

  void Observe(aura::Window* window) {
    if (window_observations_.IsObservingSource(window)) {
      return;
    }

    window_observations_.AddObservation(window);
  }

  void Reset() { window_observations_.RemoveAllObservations(); }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != ash::kClientAccessibilityIdKey) {
      return;
    }
    owner_->UpdateChildWindowIds(window);
  }

  void OnWindowAdded(aura::Window* new_window) override {
    owner_->TrackChildWindow(new_window);
  }

  void OnWindowDestroying(aura::Window* window) override {
    if (window_observations_.IsObservingSource(window)) {
      window_observations_.RemoveObservation(window);
    }
  }

 private:
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

class ArcAccessibilityTreeTracker::ArcInputMethodManagerServiceObserver
    : public ArcInputMethodManagerService::Observer {
 public:
  ArcInputMethodManagerServiceObserver(ArcAccessibilityTreeTracker* owner,
                                       Profile* const profile)
      : owner_(owner) {
    auto* arc_ime_service =
        ArcInputMethodManagerService::GetForBrowserContext(profile);
    if (arc_ime_service)
      arc_imms_observation_.Observe(arc_ime_service);
  }

  void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) override {
    is_virtual_keyboard_shown_ = visible;
    owner_->OnAndroidVirtualKeyboardVisibilityChanged(visible);
  }

  bool is_virtual_keyboard_shown() const { return is_virtual_keyboard_shown_; }

 private:
  bool is_virtual_keyboard_shown_ = false;
  base::ScopedObservation<ArcInputMethodManagerService,
                          ArcInputMethodManagerService::Observer>
      arc_imms_observation_{this};
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
};

class ArcAccessibilityTreeTracker::MojoConnectionObserver
    : public ConnectionObserver<
          ax::android::mojom::AccessibilityHelperInstance> {
 public:
  MojoConnectionObserver(ArcAccessibilityTreeTracker* owner,
                         ArcBridgeService* const arc_bridge_service)
      : owner_(owner) {
    helper_instance_connection_observation_.Observe(
        arc_bridge_service->accessibility_helper());
  }

  void OnConnectionReady() override {
    owner_->notification_observer_ =
        std::make_unique<NotificationObserver>(owner_);
  }

  void OnConnectionClosed() override { owner_->notification_observer_.reset(); }

 private:
  base::ScopedObservation<
      ConnectionHolder<ax::android::mojom::AccessibilityHelperInstance,
                       ax::android::mojom::AccessibilityHelperHost>,
      ConnectionObserver<ax::android::mojom::AccessibilityHelperInstance>>
      helper_instance_connection_observation_{this};
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
};

// Observes (1) Addition and removal of ArcNotificationSurface, and
// (2) Removal of aura::Window corresponds to ARC notification.
class ArcAccessibilityTreeTracker::NotificationObserver
    : public ash::ArcNotificationSurfaceManager::Observer,
      public aura::WindowObserver {
 public:
  explicit NotificationObserver(ArcAccessibilityTreeTracker* owner)
      : owner_(owner) {
    auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
    if (surface_manager) {
      arc_notification_observation_.Observe(surface_manager);
    }
  }

  // ash::ArcNotificationSurfaceManager::Observer overrides:
  void OnNotificationSurfaceAdded(
      ash::ArcNotificationSurface* surface) override {
    owner_->OnNotificationSurfaceAdded(surface);

    aura::Window* window = surface->GetWindow();
    if (window && !window_observations_.IsObservingSource(window)) {
      window_observations_.AddObservation(window);
    }
  }

  void OnNotificationSurfaceRemoved(
      ash::ArcNotificationSurface* surface) override {
    owner_->OnNotificationSurfaceRemoved(surface);
  }

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override {
    if (window_observations_.IsObservingSource(window)) {
      window_observations_.RemoveObservation(window);
    }
    owner_->OnNotificationWindowRemoved(window);
  }

 private:
  base::ScopedObservation<ash::ArcNotificationSurfaceManager,
                          ash::ArcNotificationSurfaceManager::Observer>
      arc_notification_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  raw_ptr<ArcAccessibilityTreeTracker> const owner_;
};

class ArcAccessibilityTreeTracker::UmaRecorder {
 public:
  explicit UmaRecorder(ArcAccessibilityTreeTracker* tree_tracker)
      : tree_tracker_(tree_tracker) {}

  void OnArcLostFocus() {
    for (const auto& feature : start_time_)
      RecordUsageTime(feature.first, feature.second);
    start_time_.clear();
  }

  void OnArcGainedFocus() {
    for (const auto& feature : enabled_features_)
      start_time_.try_emplace(feature, ui::EventTimeForNow());
  }

  void OnEnabledFeatureChanged() {
    for (const auto& feature : kFeatures) {
      bool was_enabled = enabled_features_.contains(feature);
      bool is_enabled = IsEnabled(feature);
      if (is_enabled && !was_enabled) {
        // This feature is enabled.
        enabled_features_.insert(feature);

        RecordWindowCount(feature, tree_tracker_->GetTrackingArcWindowCount());
        if (tree_tracker_->IsArcFocused())
          start_time_.try_emplace(feature, ui::EventTimeForNow());
      } else if (!is_enabled && was_enabled) {
        // This feature is disabled.
        enabled_features_.erase(feature);

        auto itr = start_time_.find(feature);
        if (itr == start_time_.end())
          continue;
        RecordUsageTime(itr->first, itr->second);
        start_time_.erase(itr);
      }
    }
  }

  void OnWindowCreated() {
    for (const auto& feature : enabled_features_)
      RecordWindowCount(feature, 1);
  }

 private:
  enum class ArcAccessibilityFeature {
    kDockedMagnifier = 0,
    kFullscreenMagnifier = 1,
    kSelectToSpeak = 2,
    kSpokenFeedback = 3,
    kSwitchAccess = 4,
    kTalkBack = 5,
    kMaxValue = kTalkBack,
  };

  static constexpr ArcAccessibilityFeature kFeatures[6] = {
      ArcAccessibilityFeature::kDockedMagnifier,
      ArcAccessibilityFeature::kFullscreenMagnifier,
      ArcAccessibilityFeature::kSelectToSpeak,
      ArcAccessibilityFeature::kSpokenFeedback,
      ArcAccessibilityFeature::kSwitchAccess,
      ArcAccessibilityFeature::kTalkBack,
  };

  static constexpr char kUmaTimePrefix[] = "Arc.Accessibility.ActiveTime.";
  static constexpr char kUmaWindowCount[] = "Arc.Accessibility.WindowCount";

  static std::string FromEnumToString(ArcAccessibilityFeature feature) {
    switch (feature) {
      case ArcAccessibilityFeature::kDockedMagnifier:
        return "DockedMagnifier";
      case ArcAccessibilityFeature::kFullscreenMagnifier:
        return "FullscreenMagnifier";
      case ArcAccessibilityFeature::kSelectToSpeak:
        return "SelectToSpeak";
      case ArcAccessibilityFeature::kSpokenFeedback:
        return "SpokenFeedback";
      case ArcAccessibilityFeature::kSwitchAccess:
        return "SwitchAccess";
      case ArcAccessibilityFeature::kTalkBack:
        return "TalkBack";
    }
  }

  void RecordUsageTime(ArcAccessibilityFeature feature,
                       base::TimeTicks start_time) {
    base::UmaHistogramLongTimes(kUmaTimePrefix + FromEnumToString(feature),
                                ui::EventTimeForNow() - start_time);
  }

  void RecordWindowCount(ArcAccessibilityFeature feature, int count) {
    for (int i = 0; i < count; i++)
      base::UmaHistogramEnumeration(kUmaWindowCount, feature);
  }

  bool IsEnabled(ArcAccessibilityFeature feature) const {
    switch (feature) {
      case ArcAccessibilityFeature::kDockedMagnifier:
        return ash::Shell::Get()->docked_magnifier_controller()->GetEnabled();
      case ArcAccessibilityFeature::kFullscreenMagnifier:
        return ash::Shell::Get()
            ->fullscreen_magnifier_controller()
            ->IsEnabled();
      case ArcAccessibilityFeature::kSelectToSpeak:
        return ash::Shell::Get()
            ->accessibility_controller()
            ->select_to_speak()
            .enabled();
      case ArcAccessibilityFeature::kSpokenFeedback:
        return ash::Shell::Get()
                   ->accessibility_controller()
                   ->spoken_feedback()
                   .enabled() &&
               tree_tracker_->is_native_chromevox_enabled();
      case ArcAccessibilityFeature::kSwitchAccess:
        return ash::Shell::Get()
            ->accessibility_controller()
            ->switch_access()
            .enabled();
      case ArcAccessibilityFeature::kTalkBack:
        return !tree_tracker_->is_native_chromevox_enabled();
    }
  }

  base::flat_map<ArcAccessibilityFeature, base::TimeTicks> start_time_;
  std::set<ArcAccessibilityFeature> enabled_features_;
  raw_ptr<const ArcAccessibilityTreeTracker> const tree_tracker_;
};

ArcAccessibilityTreeTracker::ArcAccessibilityTreeTracker(
    ax::android::AXTreeSourceAndroid::Delegate* tree_source_delegate,
    Profile* const profile,
    const AccessibilityHelperInstanceRemoteProxy& accessibility_helper_instance,
    ArcBridgeService* const arc_bridge_service)
    : profile_(profile),
      tree_source_delegate_(tree_source_delegate),
      accessibility_helper_instance_(accessibility_helper_instance),
      windows_observer_(std::make_unique<WindowsObserver>(this)),
      child_windows_observer_(std::make_unique<ChildWindowsObserver>(this)),
      input_manager_service_observer_(
          std::make_unique<ArcInputMethodManagerServiceObserver>(this,
                                                                 profile)),
      connection_observer_(
          std::make_unique<MojoConnectionObserver>(this, arc_bridge_service)),
      uma_recorder_(std::make_unique<UmaRecorder>(this)) {}

ArcAccessibilityTreeTracker::~ArcAccessibilityTreeTracker() = default;

void ArcAccessibilityTreeTracker::OnWindowInitialized(aura::Window* window) {
  if (IsArcOrGhostWindow(window)) {
    TrackWindow(window);
  }
}

void ArcAccessibilityTreeTracker::OnWindowFocused(aura::Window* gained_focus,
                                                  aura::Window* lost_focus) {
  UpdateWindowProperties(gained_focus);

  // Transitioning with ARC and non-ARC window may need to dispatch
  // ToggleNativeChromeVoxArcSupport event.
  //  - When non-ChromeVox ARC window becomes inactive, dispatch |true|.
  //  - When non-ChromeVox ARC window becomes active, dispatch |false|.
  bool lost_arc = IsArcOrGhostWindow(lost_focus);
  bool gained_arc = IsArcOrGhostWindow(gained_focus);
  bool talkback_enabled = !native_chromevox_enabled_;
  if (talkback_enabled && lost_arc != gained_arc)
    DispatchCustomSpokenFeedbackToggled(gained_arc);

  if (gained_arc)
    uma_recorder_->OnArcGainedFocus();
  if (lost_arc)
    uma_recorder_->OnArcLostFocus();
}

void ArcAccessibilityTreeTracker::OnWindowDestroying(aura::Window* window) {
  const auto task_id_opt = GetWindowTaskId(window);
  if (!task_id_opt.has_value())
    return;

  int32_t task_id = task_id_opt.value();
  task_id_to_window_.erase(task_id);
  trees_.erase(KeyForTaskId(task_id));
  std::erase_if(window_id_to_task_id_,
                [task_id](auto it) { return it.second == task_id; });
}

void ArcAccessibilityTreeTracker::Shutdown() {
  input_manager_service_observer_.reset();
}

void ArcAccessibilityTreeTracker::OnEnabledFeatureChanged(
    ax::android::mojom::AccessibilityFilterType filter_type) {
  uma_recorder_->OnEnabledFeatureChanged();

  if (filter_type_ == filter_type)
    return;

  filter_type_ = filter_type;

  if (filter_type_ == ax::android::mojom::AccessibilityFilterType::ALL) {
    focus_change_observer_ = std::make_unique<FocusChangeObserver>(this);
    StartTrackingWindows();
  } else {
    // No longer need to track windows and trees.
    trees_.clear();
    window_id_to_task_id_.clear();
    task_id_to_window_.clear();
    focus_change_observer_.reset();

    DCHECK(aura::Env::HasInstance());
    env_observation_.Reset();
    windows_observer_->Reset();
    child_windows_observer_->Reset();
  }
}

bool ArcAccessibilityTreeTracker::EnableTree(const ui::AXTreeID& tree_id) {
  ax::android::AXTreeSourceAndroid* tree_source = GetFromTreeId(tree_id);
  if (!tree_source || !tree_source->window())
    return false;

  ax::android::mojom::AccessibilityWindowKeyPtr window_key;
  if (const std::optional<int32_t> window_id_opt =
          FindAccessibilityWindowIdRecursive(tree_source->window())) {
    window_key = ax::android::mojom::AccessibilityWindowKey::NewWindowId(
        window_id_opt.value());
  } else if (const std::optional<int32_t> task_id =
                 GetWindowTaskId(tree_source->window())) {
    window_key =
        ax::android::mojom::AccessibilityWindowKey::NewTaskId(task_id.value());
  } else {
    return false;
  }

  return accessibility_helper_instance_->RequestSendAccessibilityTree(
      std::move(window_key));
}

ax::android::AXTreeSourceAndroid*
ArcAccessibilityTreeTracker::OnAccessibilityEvent(
    const ax::android::mojom::AccessibilityEventData* const event_data) {
  DCHECK(event_data);
  bool is_notification_event = event_data->notification_key.has_value();
  if (is_notification_event) {
    const std::string& notification_key = event_data->notification_key.value();

    // This bridge must receive OnNotificationStateChanged call for the
    // notification_key before this receives an accessibility event for it.
    return GetFromKey(KeyForNotification(notification_key));
  }
  if (event_data->is_input_method_window) {
    if (!input_manager_service_observer_->is_virtual_keyboard_shown())
      return nullptr;

    exo::InputMethodSurface* input_method_surface =
        exo::InputMethodSurface::GetInputMethodSurface();
    if (!input_method_surface)
      return nullptr;

    auto key = KeyForInputMethod();
    auto* tree = GetFromKey(key);
    if (!tree) {
      tree = CreateFromKey(key, input_method_surface->host_window());
      input_method_surface->SetChildAxTreeId(tree->ax_tree_id());
    }
    CHECK(tree->window() == input_method_surface->host_window());

    return tree;
  }

  int task_id;
  if (event_data->task_id != kNoTaskId) {
    task_id = event_data->task_id;
  } else {
    const auto itr = window_id_to_task_id_.find(event_data->window_id);
    if (itr == window_id_to_task_id_.end()) {
      return nullptr;
    }
    task_id = itr->second;
  }

  const auto window_itr = task_id_to_window_.find(task_id);
  if (window_itr == task_id_to_window_.end()) {
    return nullptr;
  }

  aura::Window* window = window_itr->second;

  const auto key = KeyForTaskId(task_id);
  ax::android::AXTreeSourceAndroid* tree_source = GetFromKey(key);
  if (!tree_source) {
    tree_source = CreateFromKey(key, window);
    SetChildAxTreeIDForWindow(window, tree_source->ax_tree_id());
  }

  UpdateWindowProperties(window);
  return tree_source;
}

void ArcAccessibilityTreeTracker::OnNotificationSurfaceAdded(
    ash::ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromKey(KeyForNotification(notification_key));
  if (!tree)
    return;

  tree->set_window(surface->GetWindow());
  surface->SetAXTreeId(tree->ax_tree_id());

  // Dispatch ax::mojom::Event::kChildrenChanged to force AXNodeData of the
  // notification updated. As order of OnNotificationSurfaceAdded call is not
  // guaranteed, we are dispatching the event in both
  // ArcAccessibilityHelperBridge and ArcNotificationContentView. The event
  // needs to be dispatched after:
  // 1. ax_tree_id is set to the surface
  // 2. the surface is attached to the content view
  if (surface->IsAttached()) {
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, false);
  }
}

void ArcAccessibilityTreeTracker::OnNotificationSurfaceRemoved(
    ash::ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromKey(KeyForNotification(notification_key));
  if (!tree)
    return;

  tree->set_window(nullptr);
}

void ArcAccessibilityTreeTracker::OnNotificationWindowRemoved(
    aura::Window* window) {
  for (auto& [treeKey, tree] : trees_) {
    if (tree->window() == window) {
      // Actual clean-up is done in OnNotificationStateChanged.
      tree->set_window(nullptr);
    }
  }
}

void ArcAccessibilityTreeTracker::OnNotificationStateChanged(
    const std::string& notification_key,
    const ax::android::mojom::AccessibilityNotificationStateType& state) {
  auto key = KeyForNotification(notification_key);
  using ax::android::mojom::AccessibilityNotificationStateType;
  switch (state) {
    case AccessibilityNotificationStateType::SURFACE_CREATED: {
      ax::android::AXTreeSourceAndroid* tree_source = GetFromKey(key);
      if (tree_source)
        return;

      auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
      if (!surface_manager)
        return;

      ash::ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(notification_key);

      tree_source = CreateFromKey(std::move(key),
                                  surface ? surface->GetWindow() : nullptr);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        tree_source->ax_tree_id());
      break;
    }
    case AccessibilityNotificationStateType::SURFACE_REMOVED:
      trees_.erase(key);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        ui::AXTreeIDUnknown());
      break;
    case AccessibilityNotificationStateType::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ArcAccessibilityTreeTracker::OnAndroidVirtualKeyboardVisibilityChanged(
    bool visible) {
  // The lifetime of ax::android::AXTreeSourceAndroid should be bounded by the
  // corresponding exo window. Always using OnWindowDestroying is ideal. But it
  // seems that OnWindowDestroying sometimes not called when visually VK is made
  // invisible. We're using this callback here to destroy the tree.
  if (!visible)
    trees_.erase(KeyForInputMethod());
}

void ArcAccessibilityTreeTracker::OnToggleNativeChromeVoxArcSupport(
    bool enabled) {
  // This is dispatched from Android when ArcAccessibilityHelperService changes
  // the active screen reader on Android.
  native_chromevox_enabled_ = enabled;
  uma_recorder_->OnEnabledFeatureChanged();
  DispatchCustomSpokenFeedbackToggled(!enabled);

  // TODO(hirokisato): Don't we need to do something similar in
  // OnSetNativeChromeVoxArcSupportProcessed()?
}

void ArcAccessibilityTreeTracker::SetNativeChromeVoxArcSupport(
    bool enabled,
    SetNativeChromeVoxCallback callback) {
  aura::Window* window = GetFocusedArcWindow();
  if (!window) {
    std::move(callback).Run(SetNativeChromeVoxResponse::kFailure);
    return;
  }

  if (!GetWindowTaskId(window).has_value()) {
    std::move(callback).Run(SetNativeChromeVoxResponse::kFailure);
    return;
  }

  std::unique_ptr<aura::WindowTracker> window_tracker =
      std::make_unique<aura::WindowTracker>();
  window_tracker->Add(window);

  accessibility_helper_instance_->SetNativeChromeVoxArcSupportForFocusedWindow(
      enabled,
      base::BindOnce(
          &ArcAccessibilityTreeTracker::OnSetNativeChromeVoxArcSupportProcessed,
          base::Unretained(this), std::move(window_tracker), enabled,
          std::move(callback)));
}

void ArcAccessibilityTreeTracker::OnSetNativeChromeVoxArcSupportProcessed(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    bool enabled,
    SetNativeChromeVoxCallback callback,
    ax::android::mojom::SetNativeChromeVoxResponse response) {
  std::move(callback).Run(FromMojomResponseToAutomationResponse(response));

  if (response != ax::android::mojom::SetNativeChromeVoxResponse::SUCCESS ||
      window_tracker->windows().size() != 1) {
    return;
  }

  aura::Window* window = window_tracker->Pop();
  auto task_id = GetWindowTaskId(window);
  DCHECK(task_id);

  if (enabled) {
    talkback_enabled_task_ids_.erase(*task_id);
  } else {
    trees_.erase(KeyForTaskId(*task_id));
    talkback_enabled_task_ids_.insert(*task_id);
  }

  UpdateWindowProperties(window);
}

ax::android::AXTreeSourceAndroid* ArcAccessibilityTreeTracker::GetFromTreeId(
    const ui::AXTreeID& tree_id) const {
  for (auto it = trees_.begin(); it != trees_.end(); ++it) {
    if (it->second->ax_tree_id() == tree_id)
      return it->second.get();
  }
  return nullptr;
}

ax::android::AXTreeSourceAndroid* ArcAccessibilityTreeTracker::GetFromKey(
    const TreeKey& key) {
  auto tree_it = trees_.find(key);
  if (tree_it == trees_.end())
    return nullptr;

  return tree_it->second.get();
}

ax::android::AXTreeSourceAndroid* ArcAccessibilityTreeTracker::CreateFromKey(
    TreeKey key,
    aura::Window* window) {
  auto tree = std::make_unique<ax::android::AXTreeSourceAndroid>(
      tree_source_delegate_, std::make_unique<ArcSerializationDelegate>(),
      window);
  auto [itr, inserted] = trees_.try_emplace(std::move(key), std::move(tree));
  DCHECK(inserted);
  return itr->second.get();
}

void ArcAccessibilityTreeTracker::InvalidateTrees() {
  for (auto it = trees_.begin(); it != trees_.end(); ++it)
    it->second->InvalidateTree();
}

int ArcAccessibilityTreeTracker::GetTrackingArcWindowCount() const {
  return windows_observer_->GetTrackingWindowCount();
}

bool ArcAccessibilityTreeTracker::IsArcFocused() const {
  return GetFocusedArcWindow();
}

void ArcAccessibilityTreeTracker::TrackWindow(aura::Window* window) {
  windows_observer_->Observe(window);
  UpdateTopWindowIds(window);
  UpdateWindowProperties(window);
  uma_recorder_->OnWindowCreated();
}

void ArcAccessibilityTreeTracker::TrackChildWindow(aura::Window* window) {
  child_windows_observer_->Observe(window);
  UpdateChildWindowIds(window);

  for (aura::Window* child : window->children()) {
    TrackChildWindow(child);
  }
}

void ArcAccessibilityTreeTracker::UpdateTopWindowIds(aura::Window* window) {
  auto task_id = GetWindowTaskId(window);
  if (!task_id.has_value())
    return;

  if (task_id_to_window_.contains(task_id.value())) {
    // We already know this task id.
    return;
  }
  task_id_to_window_.emplace(task_id.value(), window);

  // Force re-evaluate children so that window_id and task_id are correctly
  // mapped.
  for (aura::Window* child : window->children()) {
    TrackChildWindow(child);
  }
}

void ArcAccessibilityTreeTracker::UpdateChildWindowIds(aura::Window* window) {
  const auto window_id = exo::GetShellClientAccessibilityId(window);
  if (!window_id.has_value()) {
    return;
  }
  if (window_id_to_task_id_.find(*window_id) != window_id_to_task_id_.end()) {
    // We already know this window ID.
    return;
  }

  aura::Window* parent = FindArcWindow(window);
  auto task_id = GetWindowTaskId(parent);
  if (!task_id.has_value()) {
    return;
  }

  window_id_to_task_id_[*window_id] = *task_id;

  // The window ID is new to us. Request the entire tree.
  ax::android::mojom::AccessibilityWindowKeyPtr window_key =
      ax::android::mojom::AccessibilityWindowKey::NewWindowId(*window_id);
  accessibility_helper_instance_->RequestSendAccessibilityTree(
      std::move(window_key));
}

void ArcAccessibilityTreeTracker::UpdateWindowProperties(aura::Window* window) {
  if (!ash::IsArcWindow(window))
    return;

  auto task_id = GetWindowTaskId(window);
  if (!task_id.has_value())
    return;

  bool use_talkback = talkback_enabled_task_ids_.contains(*task_id);

  window->SetProperty(aura::client::kAccessibilityTouchExplorationPassThrough,
                      use_talkback);
  window->SetProperty(ash::kSearchKeyAcceleratorReservedKey, use_talkback);

  if (use_talkback) {
    SetChildAxTreeIDForWindow(window, ui::AXTreeIDUnknown());
  } else if (filter_type_ == ax::android::mojom::AccessibilityFilterType::ALL) {
    auto key = KeyForTaskId(*task_id);
    ax::android::AXTreeSourceAndroid* tree = GetFromKey(key);
    if (!tree)
      tree = CreateFromKey(std::move(key), window);

    // Just after the creation of window, widget has not been set yet and this
    // is not dispatched to ShellSurfaceBase. Thus, call this every time.
    SetChildAxTreeIDForWindow(window, tree->ax_tree_id());
  }
}

void ArcAccessibilityTreeTracker::StartTrackingWindows() {
  DCHECK(aura::Env::HasInstance());
  env_observation_.Observe(aura::Env::GetInstance());

  for (aura::WindowTreeHost* host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    StartTrackingWindows(host->window());
  }
}

void ArcAccessibilityTreeTracker::StartTrackingWindows(aura::Window* window) {
  if (IsArcOrGhostWindow(window)) {
    TrackWindow(window);
    return;
  }
  for (aura::Window* child : window->children())
    StartTrackingWindows(child);
}

void ArcAccessibilityTreeTracker::DispatchCustomSpokenFeedbackToggled(
    bool enabled) {
  auto event_args(extensions::api::accessibility_private::
                      OnCustomSpokenFeedbackToggled::Create(enabled));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::
          ACCESSIBILITY_PRIVATE_ON_CUSTOM_SPOKEN_FEEDBACK_TOGGLED,
      extensions::api::accessibility_private::OnCustomSpokenFeedbackToggled::
          kEventName,
      std::move(event_args));
  extensions::EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

aura::Window* ArcAccessibilityTreeTracker::GetFocusedArcWindow() const {
  if (!exo::WMHelper::HasInstance())
    return nullptr;
  return FindArcOrGhostWindow(exo::WMHelper::GetInstance()->GetFocusedWindow());
}

}  // namespace arc
