// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/system/message_center/arc/arc_notification_surface.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chromeos/chromeos_features.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

using ash::ArcNotificationSurface;
using ash::ArcNotificationSurfaceManager;

namespace {

constexpr int32_t kNoTaskId = -1;

exo::Surface* GetArcSurface(const aura::Window* window) {
  if (!window)
    return nullptr;

  exo::Surface* arc_surface = exo::Surface::AsSurface(window);
  if (!arc_surface)
    arc_surface = exo::ShellSurface::GetMainSurface(window);
  return arc_surface;
}

int32_t GetTaskId(aura::Window* window) {
  const std::string* arc_app_id = exo::ShellSurface::GetApplicationId(window);
  if (!arc_app_id)
    return kNoTaskId;

  int32_t task_id = kNoTaskId;
  if (sscanf(arc_app_id->c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return kNoTaskId;

  return task_id;
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data,
                         Profile* profile) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager || accessibility_manager->profile() != profile)
    return;

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return;

  aura::Window* active_window = wm_helper->GetActiveWindow();
  if (!active_window)
    return;

  aura::Window* toplevel_window = active_window->GetToplevelWindow();

  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->bounds_in_screen,
      1.0f / toplevel_window->layer()->device_scale_factor());

  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(widget);

  // On Android side, content is rendered without considering height of
  // caption bar, e.g. Content is rendered at y:0 instead of y:32 where 32 is
  // height of caption bar. Add back height of caption bar here.
  if (widget->IsMaximized()) {
    bounds_in_screen.Offset(
        0, static_cast<int>(static_cast<float>(widget->non_client_view()
                                                   ->frame_view()
                                                   ->GetBoundsForClientView()
                                                   .y())));
  }

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

}  // namespace

namespace arc {

namespace {

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcAccessibilityHelperBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAccessibilityHelperBridge,
          ArcAccessibilityHelperBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAccessibilityHelperBridgeFactory";

  static ArcAccessibilityHelperBridgeFactory* GetInstance() {
    return base::Singleton<ArcAccessibilityHelperBridgeFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      ArcAccessibilityHelperBridgeFactory>;

  ArcAccessibilityHelperBridgeFactory() {
    // ArcAccessibilityHelperBridge needs to track task creation and
    // destruction in the container, which are notified to ArcAppListPrefs
    // via Mojo.
    DependsOn(ArcAppListPrefsFactory::GetInstance());
  }
  ~ArcAccessibilityHelperBridgeFactory() override = default;
};

}  // namespace

// static
ArcAccessibilityHelperBridge*
ArcAccessibilityHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAccessibilityHelperBridgeFactory::GetForBrowserContext(context);
}

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      arc_bridge_service_(arc_bridge_service) {
  arc_bridge_service_->accessibility_helper()->SetHost(this);
  arc_bridge_service_->accessibility_helper()->AddObserver(this);

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() = default;

void ArcAccessibilityHelperBridge::SetNativeChromeVoxArcSupport(bool enabled) {
  aura::Window* window = GetActiveWindow();
  if (!window)
    return;
  int32_t task_id = GetTaskId(window);
  if (task_id == kNoTaskId)
    return;

  std::unique_ptr<aura::WindowTracker> window_tracker =
      std::make_unique<aura::WindowTracker>();
  window_tracker->Add(window);

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->accessibility_helper(),
                                  SetNativeChromeVoxArcSupportForFocusedWindow);
  instance->SetNativeChromeVoxArcSupportForFocusedWindow(
      enabled, base::BindOnce(&ArcAccessibilityHelperBridge::
                                  OnSetNativeChromeVoxArcSupportProcessed,
                              base::Unretained(this), std::move(window_tracker),
                              enabled));
}

void ArcAccessibilityHelperBridge::OnSetNativeChromeVoxArcSupportProcessed(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    bool enabled,
    bool processed) {
  if (!processed || window_tracker->windows().size() != 1)
    return;

  aura::Window* window = window_tracker->Pop();
  int32_t task_id = GetTaskId(window);
  DCHECK_NE(task_id, kNoTaskId);

  if (!enabled) {
    task_id_to_tree_.erase(task_id);

    exo::Surface* surface = exo::ShellSurfaceBase::GetMainSurface(window);
    if (surface) {
      views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
      static_cast<exo::ShellSurfaceBase*>(widget->widget_delegate())
          ->SetChildAxTreeId(ui::AXTreeIDUnknown());
    }
  }

  UpdateWindowProperties(window);
}

void ArcAccessibilityHelperBridge::Shutdown() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->RemoveObserver(this);

  arc_bridge_service_->accessibility_helper()->RemoveObserver(this);
  arc_bridge_service_->accessibility_helper()->SetHost(nullptr);
}

void ArcAccessibilityHelperBridge::OnConnectionReady() {
  UpdateFilterType();

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged,
            base::Unretained(this)));
    SetExploreByTouchEnabled(accessibility_manager->IsSpokenFeedbackEnabled());
  }

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->AddObserver(this);
}

void ArcAccessibilityHelperBridge::OnConnectionClosed() {
  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->RemoveObserver(this);
}

void ArcAccessibilityHelperBridge::OnAccessibilityEventDeprecated(
    mojom::AccessibilityEventType event_type,
    mojom::AccessibilityNodeInfoDataPtr event_source) {
  if (event_type == arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    DispatchFocusChange(event_source.get(), profile_);
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  // TODO(yawano): Handle AccessibilityFilterType::OFF.
  arc::mojom::AccessibilityFilterType filter_type =
      GetFilterTypeForProfile(profile_);

  if (filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME) {
    if (event_data->node_data.empty())
      return;

    AXTreeSourceArc* tree_source = nullptr;
    bool is_notification_event = event_data->notification_key.has_value();
    if (is_notification_event) {
      const std::string& notification_key =
          event_data->notification_key.value();

      // This bridge must receive OnNotificationStateChanged call for the
      // notification_key before this receives an accessibility event for it.
      tree_source = GetFromNotificationKey(notification_key);
      DCHECK(tree_source);
    } else {
      if (event_data->task_id == kNoTaskId)
        return;

      aura::Window* active_window = GetActiveWindow();
      if (!active_window)
        return;

      int32_t task_id = GetTaskId(active_window);
      if (task_id != event_data->task_id)
        return;

      tree_source = GetFromTaskId(event_data->task_id);

      if (!tree_source) {
        tree_source = CreateFromTaskId(event_data->task_id);

        ui::AXTreeData tree_data;
        tree_source->GetTreeData(&tree_data);
        exo::Surface* surface =
            exo::ShellSurfaceBase::GetMainSurface(active_window);
        if (surface) {
          views::Widget* widget =
              views::Widget::GetWidgetForNativeWindow(active_window);
          static_cast<exo::ShellSurfaceBase*>(widget->widget_delegate())
              ->SetChildAxTreeId(tree_data.tree_id);
        }
      }
    }

    if (!tree_source)
      return;

    tree_source->NotifyAccessibilityEvent(event_data.get());

    if (is_notification_event &&
        event_data->event_type ==
            arc::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED) {
      // If text selection changed event is dispatched from Android, it
      // means that user is trying to type a text in Android notification.
      // Dispatch text selection changed event to notification content view
      // as the view can take necessary actions, e.g. activate itself, etc.
      auto* surface_manager = ArcNotificationSurfaceManager::Get();
      if (surface_manager) {
        ArcNotificationSurface* surface = surface_manager->GetArcSurface(
            event_data->notification_key.value());
        if (surface) {
          surface->GetAttachedHost()->NotifyAccessibilityEvent(
              ax::mojom::Event::kTextSelectionChanged, true);
        }
      }
    } else if (!is_notification_event &&
               event_data->event_type ==
                   arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
      UpdateWindowProperties(GetActiveWindow());
    }

    return;
  }

  if (event_data->event_type !=
      arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    return;

  CHECK_EQ(1U, event_data.get()->node_data.size());
  DispatchFocusChange(event_data.get()->node_data[0].get(), profile_);
}

void ArcAccessibilityHelperBridge::OnNotificationStateChanged(
    const std::string& notification_key,
    arc::mojom::AccessibilityNotificationStateType state) {
  switch (state) {
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED: {
      AXTreeSourceArc* tree_source =
          CreateFromNotificationKey(notification_key);

      ui::AXTreeData tree_data;
      if (!tree_source->GetTreeData(&tree_data)) {
        NOTREACHED();
        return;
      }
      UpdateTreeIdOfNotificationSurface(notification_key, tree_data.tree_id);
      break;
    }
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED:
      notification_key_to_tree_.erase(notification_key);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        ui::AXTreeIDUnknown());
      break;
  }
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromTaskId(int32_t task_id) {
  auto tree_it = task_id_to_tree_.find(task_id);
  if (tree_it == task_id_to_tree_.end())
    return nullptr;

  return tree_it->second.get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::CreateFromTaskId(
    int32_t task_id) {
  task_id_to_tree_[task_id].reset(new AXTreeSourceArc(this));
  return task_id_to_tree_[task_id].get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::CreateFromNotificationKey(
    const std::string& notification_key) {
  notification_key_to_tree_[notification_key].reset(new AXTreeSourceArc(this));
  return notification_key_to_tree_[notification_key].get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromNotificationKey(
    const std::string& notification_key) {
  const auto tree_it = notification_key_to_tree_.find(notification_key);
  if (tree_it == notification_key_to_tree_.end())
    return nullptr;

  return tree_it->second.get();
}

void ArcAccessibilityHelperBridge::UpdateTreeIdOfNotificationSurface(
    const std::string& notification_key,
    ui::AXTreeID tree_id) {
  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (!surface_manager)
    return;

  ArcNotificationSurface* surface =
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

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromTreeId(
    ui::AXTreeID tree_id) const {
  for (auto it = task_id_to_tree_.begin(); it != task_id_to_tree_.end(); ++it) {
    ui::AXTreeData tree_data;
    it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return it->second.get();
  }

  for (auto notification_it = notification_key_to_tree_.begin();
       notification_it != notification_key_to_tree_.end(); ++notification_it) {
    ui::AXTreeData tree_data;
    notification_it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return notification_it->second.get();
  }

  return nullptr;
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
  arc::mojom::AccessibilityActionDataPtr action_data =
      arc::mojom::AccessibilityActionData::New();

  action_data->node_id = data.target_node_id;

  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);
  if (!tree_source)
    return;
  action_data->window_id = tree_source->window_id();

  switch (data.action) {
    case ax::mojom::Action::kDoDefault:
      action_data->action_type = arc::mojom::AccessibilityActionType::CLICK;
      break;
    case ax::mojom::Action::kFocus:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SHOW_ON_SCREEN;
      break;
    case ax::mojom::Action::kScrollBackward:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_BACKWARD;
      break;
    case ax::mojom::Action::kScrollForward:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_FORWARD;
      break;
    case ax::mojom::Action::kScrollUp:
      action_data->action_type = arc::mojom::AccessibilityActionType::SCROLL_UP;
      break;
    case ax::mojom::Action::kScrollDown:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_DOWN;
      break;
    case ax::mojom::Action::kScrollLeft:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_LEFT;
      break;
    case ax::mojom::Action::kScrollRight:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::SCROLL_RIGHT;
      break;
    case ax::mojom::Action::kCustomAction:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::CUSTOM_ACTION;
      action_data->custom_action_id = data.custom_action_id;
      break;
    case ax::mojom::Action::kSetAccessibilityFocus:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
      break;
    case ax::mojom::Action::kClearAccessibilityFocus:
      action_data->action_type =
          arc::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS;
      break;
    default:
      return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), PerformAction);
  instance->PerformAction(
      std::move(action_data),
      base::BindOnce(&ArcAccessibilityHelperBridge::OnActionResult,
                     base::Unretained(this), data));
}

void ArcAccessibilityHelperBridge::OnActionResult(const ui::AXActionData& data,
                                                  bool result) const {
  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyActionResult(data, result);
}

void ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& event_details) {
  // TODO(yawano): Add case for select to speak and switch access.
  if (event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_FOCUS_HIGHLIGHT) {
    return;
  }

  UpdateFilterType();
  UpdateWindowProperties(GetActiveWindow());

  if (event_details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    SetExploreByTouchEnabled(event_details.enabled);
  }
}

arc::mojom::AccessibilityFilterType
ArcAccessibilityHelperBridge::GetFilterTypeForProfile(Profile* profile) {
  if (use_filter_type_all_for_test_)
    return arc::mojom::AccessibilityFilterType::ALL;

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!accessibility_manager)
    return arc::mojom::AccessibilityFilterType::OFF;

  // TODO(yawano): Support the case where primary user is in background.
  if (accessibility_manager->profile() != profile)
    return arc::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  if (accessibility_manager->IsSpokenFeedbackEnabled()) {
    return base::FeatureList::IsEnabled(
               chromeos::features::kChromeVoxArcSupport)
               ? arc::mojom::AccessibilityFilterType::ALL
               : arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME;
  }

  if (accessibility_manager->IsFocusHighlightEnabled())
    return arc::mojom::AccessibilityFilterType::FOCUS;

  return arc::mojom::AccessibilityFilterType::OFF;
}

void ArcAccessibilityHelperBridge::UpdateFilterType() {
  arc::mojom::AccessibilityFilterType filter_type =
      GetFilterTypeForProfile(profile_);

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetFilter);
  if (instance)
    instance->SetFilter(filter_type);

  bool add_activation_observer =
      filter_type == arc::mojom::AccessibilityFilterType::ALL ||
      filter_type ==
          arc::mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME;
  if (add_activation_observer == activation_observer_added_)
    return;

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return;

  if (add_activation_observer) {
    wm_helper->AddActivationObserver(this);
    activation_observer_added_ = true;
  } else {
    activation_observer_added_ = false;
    wm_helper->RemoveActivationObserver(this);
  }
}

void ArcAccessibilityHelperBridge::UpdateWindowProperties(
    aura::Window* window) {
  if (!window)
    return;

  if (!GetArcSurface(window))
    return;

  // First, do a lookup for the task id associated with this app. There should
  // always be a valid entry.
  int32_t task_id = GetTaskId(window);

  // Do a lookup for the tree source. A tree source may not exist because the
  // app isn't whitelisted Android side or no data has been received for the
  // app.
  auto it = task_id_to_tree_.find(task_id);
  bool use_talkback = it == task_id_to_tree_.end();

  window->SetProperty(aura::client::kAccessibilityTouchExplorationPassThrough,
                      use_talkback);
  window->SetProperty(ash::kSearchKeyAcceleratorReservedKey, use_talkback);
  window->SetProperty(aura::client::kAccessibilityFocusFallsbackToWidgetKey,
                      !use_talkback);
}

void ArcAccessibilityHelperBridge::SetExploreByTouchEnabled(bool enabled) {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetExploreByTouchEnabled);
  if (instance)
    instance->SetExploreByTouchEnabled(enabled);
}

aura::Window* ArcAccessibilityHelperBridge::GetActiveWindow() {
  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return nullptr;

  return wm_helper->GetActiveWindow();
}

void ArcAccessibilityHelperBridge::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  UpdateWindowProperties(gained_active);
}

void ArcAccessibilityHelperBridge::OnTaskDestroyed(int32_t task_id) {
  task_id_to_tree_.erase(task_id);
}

void ArcAccessibilityHelperBridge::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromNotificationKey(notification_key);
  if (!tree)
    return;

  ui::AXTreeData tree_data;
  if (!tree->GetTreeData(&tree_data))
    return;

  surface->SetAXTreeId(tree_data.tree_id);

  // Dispatch ax::mojom::Event::kChildrenChanged to force AXNodeData of the
  // notification updated. As order of OnNotificationSurfaceAdded call is not
  // guaranteed, we are dispatching the event in both
  // ArcAccessibilityHelperBridge and ArcNotificationContentView. The event
  // needs to be dispatched after 1. ax tree id is set to the surface, 2 the
  // surface is attached to the content view.
  if (surface->IsAttached()) {
    surface->GetAttachedHost()->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, false);
  }
}

}  // namespace arc
