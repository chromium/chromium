// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/accessibility/geometry_util.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/pref_names.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/controls/native/native_view_host.h"

namespace arc {
namespace {

using ::ash::AccessibilityManager;
using ::ash::AccessibilityNotificationType;
using ::ash::MagnificationManager;

// ClassName for toast from ARC++ R onwards.
constexpr char kToastEventSourceArcR[] = "android.widget.Toast";
// TODO(sarakato): Remove this once ARC++ P has been deprecated.
constexpr char kToastEventSourceArcP[] = "android.widget.Toast$TN";

bool ShouldAnnounceEvent(
    ax::android::mojom::AccessibilityEventData* event_data) {
  if (event_data->event_type ==
      ax::android::mojom::AccessibilityEventType::ANNOUNCEMENT) {
    return true;
  } else if (event_data->event_type ==
             ax::android::mojom::AccessibilityEventType::
                 NOTIFICATION_STATE_CHANGED) {
    // Only announce the event from toast.
    if (!event_data->string_properties)
      return false;

    auto it = event_data->string_properties->find(
        ax::android::mojom::AccessibilityEventStringProperty::CLASS_NAME);
    if (it == event_data->string_properties->end())
      return false;

    return (it->second == kToastEventSourceArcP) ||
           (it->second == kToastEventSourceArcR);
  }
  return false;
}

void DispatchFocusChange(
    ax::android::mojom::AccessibilityNodeInfoData* node_data,
    Profile* profile) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (!node_data || !accessibility_manager ||
      accessibility_manager->profile() != profile)
    return;

  DCHECK(exo::WMHelper::HasInstance());
  aura::Window* active_window = exo::WMHelper::GetInstance()->GetActiveWindow();
  if (!active_window)
    return;

  // Convert bounds from Android pixels to Chrome DIP, and adjust coordinate to
  // Chrome's screen coordinate.
  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->bounds_in_screen,
      1.0f / exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(
                 active_window));
  bounds_in_screen.Offset(0, GetChromeWindowHeightOffsetInDip(active_window));

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(active_window);
  bounds_in_screen.Offset(display.bounds().x(), display.bounds().y());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

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

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override { return true; }

 private:
  friend struct base::DefaultSingletonTraits<
      ArcAccessibilityHelperBridgeFactory>;

  ArcAccessibilityHelperBridgeFactory() {
    // ArcAccessibilityHelperBridge needs to track task creation and
    // destruction in the container, which are notified to ArcAppListPrefs
    // via Mojo.
    DependsOn(ArcAppListPrefsFactory::GetInstance());

    // ArcAccessibilityHelperBridge needs to track visibility change of Android
    // keyboard to delete its accessibility tree when it becomes hidden.
    DependsOn(ArcInputMethodManagerService::GetFactory());
  }
  ~ArcAccessibilityHelperBridgeFactory() override = default;
};

}  // namespace

// static
void ArcAccessibilityHelperBridge::CreateFactory() {
  ArcAccessibilityHelperBridgeFactory::GetInstance();
}

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
      arc_bridge_service_(arc_bridge_service),
      accessibility_helper_instance_(arc_bridge_service_),
      tree_tracker_(this,
                    profile_,
                    accessibility_helper_instance_,
                    arc_bridge_service_) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(
      Profile::FromBrowserContext(browser_context)->GetPrefs());

  arc_bridge_service_->accessibility_helper()->SetHost(this);
  arc_bridge_service_->accessibility_helper()->AddObserver(this);

  automation_event_router_observer_.Observe(
      extensions::AutomationEventRouter::GetInstance());
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() = default;

void ArcAccessibilityHelperBridge::SetNativeChromeVoxArcSupport(
    bool enabled,
    SetNativeChromeVoxCallback callback) {
  tree_tracker_.SetNativeChromeVoxArcSupport(enabled, std::move(callback));
}

bool ArcAccessibilityHelperBridge::EnableTree(const ui::AXTreeID& tree_id) {
  return tree_tracker_.EnableTree(tree_id);
}

void ArcAccessibilityHelperBridge::Shutdown() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  arc_bridge_service_->accessibility_helper()->RemoveObserver(this);
  arc_bridge_service_->accessibility_helper()->SetHost(nullptr);

  tree_tracker_.Shutdown();
}

void ArcAccessibilityHelperBridge::OnConnectionReady() {
  UpdateEnabledFeature();

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged,
            base::Unretained(this)));
    accessibility_helper_instance_.SetExploreByTouchEnabled(
        accessibility_manager->IsSpokenFeedbackEnabled());
  }
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    ax::android::mojom::AccessibilityEventDataPtr event_data) {
  filter_type_ = GetFilterType();
  switch (filter_type_) {
    case ax::android::mojom::AccessibilityFilterType::ALL:
      HandleFilterTypeAllEvent(std::move(event_data));
      break;
    case ax::android::mojom::AccessibilityFilterType::FOCUS:
      HandleFilterTypeFocusEvent(std::move(event_data));
      break;
    case ax::android::mojom::AccessibilityFilterType::OFF:
      break;
    case ax::android::mojom::AccessibilityFilterType::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ArcAccessibilityHelperBridge::OnNotificationStateChanged(
    const std::string& notification_key,
    ax::android::mojom::AccessibilityNotificationStateType state) {
  tree_tracker_.OnNotificationStateChanged(std::move(notification_key),
                                           std::move(state));
}

void ArcAccessibilityHelperBridge::OnToggleNativeChromeVoxArcSupport(
    bool enabled) {
  tree_tracker_.OnToggleNativeChromeVoxArcSupport(enabled);
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
  DCHECK(data.target_node_id);

  ax::android::AXTreeSourceAndroid* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);
  if (!tree_source)
    return;

  std::optional<int32_t> window_id = tree_source->window_id();
  if (!window_id)
    return;

  const std::optional<ax::android::mojom::AccessibilityActionType> action =
      ax::android::ConvertToAndroidAction(data.action);
  if (!action.has_value())
    return;

  ax::android::mojom::AccessibilityActionDataPtr action_data =
      ax::android::mojom::AccessibilityActionData::New();

  action_data->node_id = data.target_node_id;
  action_data->window_id = window_id.value();
  action_data->action_type = action.value();
  PopulateActionParameters(data, *action_data);

  if (action ==
      ax::android::mojom::AccessibilityActionType::GET_TEXT_LOCATION) {
    action_data->start_index = data.start_index;
    action_data->end_index = data.end_index;
    if (!accessibility_helper_instance_.RefreshWithExtraData(
            std::move(action_data),
            base::BindOnce(
                &ArcAccessibilityHelperBridge::OnGetTextLocationDataResult,
                base::Unretained(this), data))) {
      OnActionResult(data, false);
    }
    return;
  }

  if (!accessibility_helper_instance_.PerformAction(
          std::move(action_data),
          base::BindOnce(&ArcAccessibilityHelperBridge::OnActionResult,
                         base::Unretained(this), data))) {
    // TODO(b/146809329): This case should probably destroy all trees.
    OnActionResult(data, false);
  }
}

void ArcAccessibilityHelperBridge::PopulateActionParameters(
    const ui::AXActionData& chrome_data,
    ax::android::mojom::AccessibilityActionData& action_data) const {
  switch (action_data.action_type) {
    case ax::android::mojom::AccessibilityActionType::SCROLL_TO_POSITION: {
      base::flat_map<ax::android::mojom::ActionIntArgumentType, int32_t> args;
      const auto [row, column] = chrome_data.row_column;
      args[ax::android::mojom::ActionIntArgumentType::ROW_INT] = row;
      args[ax::android::mojom::ActionIntArgumentType::COLUMN_INT] = column;
      action_data.int_parameters = args;
      break;
    }
    case ax::android::mojom::AccessibilityActionType::CUSTOM_ACTION:
      action_data.custom_action_id = chrome_data.custom_action_id;
      break;
    case ax::android::mojom::AccessibilityActionType::NEXT_HTML_ELEMENT:
    case ax::android::mojom::AccessibilityActionType::PREVIOUS_HTML_ELEMENT:
    case ax::android::mojom::AccessibilityActionType::FOCUS:
    case ax::android::mojom::AccessibilityActionType::CLEAR_FOCUS:
    case ax::android::mojom::AccessibilityActionType::SELECT:
    case ax::android::mojom::AccessibilityActionType::CLEAR_SELECTION:
    case ax::android::mojom::AccessibilityActionType::CLICK:
    case ax::android::mojom::AccessibilityActionType::LONG_CLICK:
    case ax::android::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS:
    case ax::android::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS:
    case ax::android::mojom::AccessibilityActionType::
        NEXT_AT_MOVEMENT_GRANULARITY:
    case ax::android::mojom::AccessibilityActionType::
        PREVIOUS_AT_MOVEMENT_GRANULARITY:
    case ax::android::mojom::AccessibilityActionType::SCROLL_FORWARD:
    case ax::android::mojom::AccessibilityActionType::SCROLL_BACKWARD:
    case ax::android::mojom::AccessibilityActionType::COPY:
    case ax::android::mojom::AccessibilityActionType::PASTE:
    case ax::android::mojom::AccessibilityActionType::CUT:
    case ax::android::mojom::AccessibilityActionType::SET_SELECTION:
    case ax::android::mojom::AccessibilityActionType::EXPAND:
    case ax::android::mojom::AccessibilityActionType::COLLAPSE:
    case ax::android::mojom::AccessibilityActionType::DISMISS:
    case ax::android::mojom::AccessibilityActionType::SET_TEXT:
    case ax::android::mojom::AccessibilityActionType::CONTEXT_CLICK:
    case ax::android::mojom::AccessibilityActionType::SCROLL_DOWN:
    case ax::android::mojom::AccessibilityActionType::SCROLL_LEFT:
    case ax::android::mojom::AccessibilityActionType::SCROLL_RIGHT:
    case ax::android::mojom::AccessibilityActionType::SCROLL_UP:
    case ax::android::mojom::AccessibilityActionType::SET_PROGRESS:
    case ax::android::mojom::AccessibilityActionType::SHOW_ON_SCREEN:
    case ax::android::mojom::AccessibilityActionType::GET_TEXT_LOCATION:
    case ax::android::mojom::AccessibilityActionType::SHOW_TOOLTIP:
    case ax::android::mojom::AccessibilityActionType::HIDE_TOOLTIP:
      break;
    case ax::android::mojom::AccessibilityActionType::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool ArcAccessibilityHelperBridge::UseFullFocusMode() const {
  return use_full_focus_mode_;
}

void ArcAccessibilityHelperBridge::OnNotificationSurfaceAdded(
    ash::ArcNotificationSurface* surface) {
  tree_tracker_.OnNotificationSurfaceAdded(surface);
}

void ArcAccessibilityHelperBridge::AllAutomationExtensionsGone() {
  // Extension features are directly monitored, so no work needed here.
}

void ArcAccessibilityHelperBridge::ExtensionListenerAdded() {
  tree_tracker_.InvalidateTrees();
}

extensions::EventRouter* ArcAccessibilityHelperBridge::GetEventRouter() const {
  return extensions::EventRouter::Get(profile_);
}

ax::android::mojom::AccessibilityFilterType
ArcAccessibilityHelperBridge::GetFilterType() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  const MagnificationManager* magnification_manager =
      MagnificationManager::Get();

  if (!accessibility_manager || !magnification_manager)
    return ax::android::mojom::AccessibilityFilterType::OFF;

  // TODO(yawano): Support the case where primary user is in background.
  if (accessibility_manager->profile() != profile_)
    return ax::android::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled() ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      magnification_manager->IsMagnifierEnabled() ||
      magnification_manager->IsDockedMagnifierEnabled()) {
    return ax::android::mojom::AccessibilityFilterType::ALL;
  }

  if (accessibility_manager->IsFocusHighlightEnabled()) {
    return ax::android::mojom::AccessibilityFilterType::FOCUS;
  }

  return ax::android::mojom::AccessibilityFilterType::OFF;
}

void ArcAccessibilityHelperBridge::OnActionResult(const ui::AXActionData& data,
                                                  bool result) const {
  ax::android::AXTreeSourceAndroid* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyActionResult(data, result);
}

void ArcAccessibilityHelperBridge::OnGetTextLocationDataResult(
    const ui::AXActionData& data,
    const std::optional<gfx::Rect>& result_rect) const {
  ax::android::AXTreeSourceAndroid* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyGetTextLocationDataResult(
      data,
      OnGetTextLocationDataResultInternal(data.target_tree_id, result_rect));
}

std::optional<gfx::Rect>
ArcAccessibilityHelperBridge::OnGetTextLocationDataResultInternal(
    const ui::AXTreeID& ax_tree_id,
    const std::optional<gfx::Rect>& result_rect) const {
  if (!result_rect)
    return std::nullopt;

  ax::android::AXTreeSourceAndroid* tree_source =
      tree_tracker_.GetFromTreeId(ax_tree_id);
  if (!tree_source)
    return std::nullopt;

  aura::Window* window = tree_source->window();
  if (!window)
    return std::nullopt;

  const gfx::RectF& rect_f =
      ScaleAndroidPxToChromePx(result_rect.value(), window);

  return gfx::ToEnclosingRect(rect_f);
}

void ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
          AccessibilityNotificationType::kToggleFocusHighlight &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSelectToSpeak &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSpokenFeedback &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSwitchAccess &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleDockedMagnifier &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleScreenMagnifier) {
    return;
  }

  UpdateEnabledFeature();

  if (event_details.notification_type ==
      AccessibilityNotificationType::kToggleSpokenFeedback) {
    accessibility_helper_instance_.SetExploreByTouchEnabled(
        event_details.enabled);
  }
}

void ArcAccessibilityHelperBridge::UpdateEnabledFeature() {
  filter_type_ = GetFilterType();

  // Let Android know the filter type change.
  accessibility_helper_instance_.SetFilter(filter_type_);

  const AccessibilityManager* accessibility_manager =
      AccessibilityManager::Get();
  if (accessibility_manager) {
    is_focus_event_enabled_ = accessibility_manager->IsFocusHighlightEnabled();

    use_full_focus_mode_ = accessibility_manager->IsSwitchAccessEnabled() ||
                           accessibility_manager->IsSpokenFeedbackEnabled();
  }

  tree_tracker_.OnEnabledFeatureChanged(filter_type_);
}

void ArcAccessibilityHelperBridge::HandleFilterTypeFocusEvent(
    ax::android::mojom::AccessibilityEventDataPtr event_data) {
  if (event_data.get()->node_data.size() == 1 &&
      event_data->event_type ==
          ax::android::mojom::AccessibilityEventType::VIEW_FOCUSED) {
    DispatchFocusChange(event_data.get()->node_data[0].get(), profile_);
  }
}

void ArcAccessibilityHelperBridge::HandleFilterTypeAllEvent(
    ax::android::mojom::AccessibilityEventDataPtr event_data) {
  if (ShouldAnnounceEvent(event_data.get())) {
    DispatchEventTextAnnouncement(event_data.get());
    return;
  }

  if (event_data->node_data.empty())
    return;

  ax::android::AXTreeSourceAndroid* tree_source =
      tree_tracker_.OnAccessibilityEvent(event_data.get());
  if (!tree_source)
    return;

  tree_source->NotifyAccessibilityEvent(event_data.get());

  bool is_notification_event = event_data->notification_key.has_value();
  if (is_notification_event && event_data->event_type ==
                                   ax::android::mojom::AccessibilityEventType::
                                       VIEW_TEXT_SELECTION_CHANGED) {
    // If text selection changed event is dispatched from Android, it
    // means that user is trying to type a text in Android notification.
    // Dispatch text selection changed event to notification content view
    // as the view can take necessary actions, e.g. activate itself, etc.
    auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
    if (surface_manager) {
      ash::ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(event_data->notification_key.value());
      if (surface) {
        surface->GetAttachedHost()->NotifyAccessibilityEvent(
            ax::mojom::Event::kTextSelectionChanged, true);
      }
    }
  }

  if (is_focus_event_enabled_ &&
      event_data->event_type ==
          ax::android::mojom::AccessibilityEventType::VIEW_FOCUSED) {
    for (size_t i = 0; i < event_data->node_data.size(); ++i) {
      if (event_data->node_data[i]->id == event_data->source_id) {
        DispatchFocusChange(event_data->node_data[i].get(), profile_);
        break;
      }
    }
  }
}

void ArcAccessibilityHelperBridge::DispatchEventTextAnnouncement(
    ax::android::mojom::AccessibilityEventData* event_data) const {
  if (!event_data->event_text.has_value())
    return;

  auto event_args(
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          Create(*(event_data->event_text)));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ANNOUNCE_FOR_ACCESSIBILITY,
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          kEventName,
      std::move(event_args)));
  GetEventRouter()->BroadcastEvent(std::move(event));
}

// static
void ArcAccessibilityHelperBridge::EnsureFactoryBuilt() {
  ArcAccessibilityHelperBridgeFactory::GetInstance();
}

}  // namespace arc
