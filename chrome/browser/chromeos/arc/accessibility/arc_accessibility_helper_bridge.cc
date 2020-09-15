// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/chromeos/arc/accessibility/geometry_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

using ash::ArcNotificationSurface;
using ash::ArcNotificationSurfaceManager;

namespace {

// ClassName for toast from ARC++ R onwards.
constexpr char kToastEventSourceArcR[] = "android.widget.Toast";
// TODO(sarakato): Remove this once ARC++ P has been deprecated.
constexpr char kToastEventSourceArcP[] = "android.widget.Toast$TN";

bool ShouldAnnounceEvent(arc::mojom::AccessibilityEventData* event_data) {
  if (event_data->event_type ==
      arc::mojom::AccessibilityEventType::ANNOUNCEMENT) {
    return true;
  } else if (event_data->event_type ==
             arc::mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED) {
    // Only announce the event from toast.
    if (!event_data->string_properties)
      return false;

    auto it = event_data->string_properties->find(
        arc::mojom::AccessibilityEventStringProperty::CLASS_NAME);
    if (it == event_data->string_properties->end())
      return false;

    return (it->second == kToastEventSourceArcP) ||
           (it->second == kToastEventSourceArcR);
  }
  return false;
}

float DeviceScaleFactorFromWindow(aura::Window* window) {
  if (!window || !window->GetToplevelWindow())
    return 1.0;
  return window->GetToplevelWindow()->layer()->device_scale_factor();
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data,
                         Profile* profile) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (!node_data || !accessibility_manager ||
      accessibility_manager->profile() != profile)
    return;

  DCHECK(exo::WMHelper::HasInstance());
  aura::Window* active_window = exo::WMHelper::GetInstance()->GetActiveWindow();
  if (!active_window)
    return;

  gfx::Rect bounds_in_screen = gfx::ToEnclosingRect(arc::ToChromeBounds(
      node_data->bounds_in_screen,
      views::Widget::GetWidgetForNativeView(active_window)));

  bool is_editable = arc::GetBooleanProperty(
      node_data, arc::mojom::AccessibilityBooleanProperty::EDITABLE);

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen, is_editable);
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

static constexpr const char* kTextShadowRaised =
    "-2px -2px 4px rgba(0, 0, 0, 0.5)";
static constexpr const char* kTextShadowDepressed =
    "2px 2px 4px rgba(0, 0, 0, 0.5)";
static constexpr const char* kTextShadowUniform =
    "-1px 0px 0px black, 0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px "
    "black";
static constexpr const char* kTextShadowDropShadow =
    "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";

std::string GetARGBFromPrefs(PrefService* prefs,
                             const char* color_pref_name,
                             const char* opacity_pref_name) {
  const std::string color = prefs->GetString(color_pref_name);
  if (color.empty()) {
    return "";
  }
  const int opacity = prefs->GetInteger(opacity_pref_name);
  return base::StringPrintf("rgba(%s,%s)", color.c_str(),
                            base::NumberToString(opacity / 100.0).c_str());
}

ArcAccessibilityHelperBridge::TreeKey KeyForTaskId(int32_t value) {
  return {ArcAccessibilityHelperBridge::TreeKeyType::kTaskId, value, {}};
}

ArcAccessibilityHelperBridge::TreeKey KeyForNotification(std::string value) {
  return {ArcAccessibilityHelperBridge::TreeKeyType::kNotificationKey, 0,
          std::move(value)};
}

ArcAccessibilityHelperBridge::TreeKey KeyForInputMethod() {
  return {ArcAccessibilityHelperBridge::TreeKeyType::kInputMethod, 0, {}};
}

}  // namespace

arc::mojom::CaptionStylePtr GetCaptionStyleFromPrefs(PrefService* prefs) {
  DCHECK(prefs);

  arc::mojom::CaptionStylePtr style = arc::mojom::CaptionStyle::New();

  style->text_size = prefs->GetString(prefs::kAccessibilityCaptionsTextSize);
  style->text_color =
      GetARGBFromPrefs(prefs, prefs::kAccessibilityCaptionsTextColor,
                       prefs::kAccessibilityCaptionsTextOpacity);
  style->background_color =
      GetARGBFromPrefs(prefs, prefs::kAccessibilityCaptionsBackgroundColor,
                       prefs::kAccessibilityCaptionsBackgroundOpacity);
  style->user_locale = prefs->GetString(language::prefs::kApplicationLocale);

  const std::string test_shadow =
      prefs->GetString(prefs::kAccessibilityCaptionsTextShadow);
  if (test_shadow == kTextShadowRaised) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::RAISED;
  } else if (test_shadow == kTextShadowDepressed) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::DEPRESSED;
  } else if (test_shadow == kTextShadowUniform) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::UNIFORM;
  } else if (test_shadow == kTextShadowDropShadow) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::DROP_SHADOW;
  } else {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::NONE;
  }

  return style;
}

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

// static
ArcAccessibilityHelperBridge::TreeKey
ArcAccessibilityHelperBridge::KeyForNotification(std::string notification_key) {
  return arc::KeyForNotification(std::move(notification_key));
}

// The list of prefs we want to observe.
const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity,
    language::prefs::kApplicationLocale};

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      arc_bridge_service_(arc_bridge_service) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(
      Profile::FromBrowserContext(browser_context)->GetPrefs());

  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    pref_change_registrar_->Add(
        pref_name,
        base::Bind(&ArcAccessibilityHelperBridge::UpdateCaptionSettings,
                   base::Unretained(this)));
  }

  arc_bridge_service_->accessibility_helper()->SetHost(this);
  arc_bridge_service_->accessibility_helper()->AddObserver(this);

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->AddObserver(this);

  auto* arc_ime_service =
      ArcInputMethodManagerService::GetForBrowserContext(browser_context);
  if (arc_ime_service)
    arc_ime_service->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() = default;

void ArcAccessibilityHelperBridge::SetNativeChromeVoxArcSupport(bool enabled) {
  aura::Window* window = GetActiveWindow();
  if (!window)
    return;
  int32_t task_id = arc::GetWindowTaskId(window);
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
  int32_t task_id = arc::GetWindowTaskId(window);
  DCHECK_NE(task_id, kNoTaskId);

  if (enabled) {
    talkback_enabled_task_ids_.erase(task_id);
  } else {
    trees_.erase(KeyForTaskId(task_id));
    talkback_enabled_task_ids_.insert(task_id);
  }

  UpdateWindowProperties(window);
  base::UmaHistogramBoolean("Arc.AccessibilityWithTalkBack", !enabled);
}

bool ArcAccessibilityHelperBridge::RefreshTreeIfInActiveWindow(
    const ui::AXTreeID& tree_id) {
  aura::Window* active_window = GetActiveWindow();
  if (!active_window)
    return false;

  auto task_id = arc::GetWindowTaskId(active_window);
  if (task_id == kNoTaskId)
    return false;

  AXTreeSourceArc* tree_source = GetFromKey(KeyForTaskId(task_id));
  if (!tree_source || tree_source->ax_tree_id() != tree_id)
    return false;

  arc::mojom::AccessibilityWindowKeyPtr window_key =
      arc::mojom::AccessibilityWindowKey::New();
  if (exo::GetShellClientAccessibilityId(active_window).has_value()) {
    window_key->set_window_id(
        exo::GetShellClientAccessibilityId(active_window).value());
  } else {
    window_key->set_task_id(task_id);
  }

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->accessibility_helper(),
                                  RequestSendAccessibilityTree);
  if (!instance)
    return false;

  instance->RequestSendAccessibilityTree(std::move(window_key));
  return true;
}

void ArcAccessibilityHelperBridge::Shutdown() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  // Null on testing.
  auto* app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (app_list_prefs)
    app_list_prefs->RemoveObserver(this);

  auto* arc_ime_service =
      ArcInputMethodManagerService::GetForBrowserContext(profile_);
  if (arc_ime_service)
    arc_ime_service->RemoveObserver(this);

  arc_bridge_service_->accessibility_helper()->RemoveObserver(this);
  arc_bridge_service_->accessibility_helper()->SetHost(nullptr);
}

void ArcAccessibilityHelperBridge::OnConnectionReady() {
  UpdateEnabledFeature();
  UpdateCaptionSettings();
  UpdateWindowProperties(GetActiveWindow());

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

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  filter_type_ = GetFilterTypeForProfile(profile_);
  switch (filter_type_) {
    case arc::mojom::AccessibilityFilterType::ALL:
      HandleFilterTypeAllEvent(std::move(event_data));
      break;
    case arc::mojom::AccessibilityFilterType::FOCUS:
      HandleFilterTypeFocusEvent(std::move(event_data));
      break;
    case arc::mojom::AccessibilityFilterType::OFF:
      break;
  }
}

void ArcAccessibilityHelperBridge::OnNotificationStateChanged(
    const std::string& notification_key,
    arc::mojom::AccessibilityNotificationStateType state) {
  auto key = KeyForNotification(notification_key);
  switch (state) {
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED: {
      aura::Window* window = nullptr;
      auto* surface_manager = ArcNotificationSurfaceManager::Get();
      if (surface_manager) {
        ArcNotificationSurface* surface =
            surface_manager->GetArcSurface(notification_key);
        if (surface)
          window = surface->GetWindow();
      }

      AXTreeSourceArc* tree_source = GetFromKey(key);
      if (tree_source) {
        tree_source->set_device_scale_factor(
            DeviceScaleFactorFromWindow(window));
        return;
      }

      tree_source = CreateFromKey(std::move(key), window);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        tree_source->ax_tree_id());
      break;
    }
    case arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED:
      trees_.erase(key);
      UpdateTreeIdOfNotificationSurface(notification_key,
                                        ui::AXTreeIDUnknown());
      break;
  }
}

void ArcAccessibilityHelperBridge::OnToggleNativeChromeVoxArcSupport(
    bool enabled) {
  native_chromevox_enabled_ = enabled;
  DispatchCustomSpokenFeedbackToggled(!enabled);
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
  DCHECK(data.target_node_id);

  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);
  if (!tree_source)
    return;

  if (data.action == ax::mojom::Action::kInternalInvalidateTree) {
    tree_source->InvalidateTree();
    return;
  }

  base::Optional<int32_t> window_id = tree_source->window_id();
  if (!window_id)
    return;

  const base::Optional<mojom::AccessibilityActionType> action =
      ConvertToAndroidAction(data.action);
  if (!action.has_value())
    return;

  arc::mojom::AccessibilityActionDataPtr action_data =
      arc::mojom::AccessibilityActionData::New();

  action_data->node_id = data.target_node_id;
  action_data->window_id = window_id.value();
  action_data->action_type = action.value();

  if (action == arc::mojom::AccessibilityActionType::GET_TEXT_LOCATION) {
    action_data->start_index = data.start_index;
    action_data->end_index = data.end_index;
    auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_bridge_service_->accessibility_helper(), RefreshWithExtraData);
    if (!instance) {
      OnActionResult(data, false);
      return;
    }
    instance->RefreshWithExtraData(
        std::move(action_data),
        base::BindOnce(
            &ArcAccessibilityHelperBridge::OnGetTextLocationDataResult,
            base::Unretained(this), data));
    return;
  } else if (action == arc::mojom::AccessibilityActionType::CUSTOM_ACTION) {
    action_data->custom_action_id = data.custom_action_id;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), PerformAction);
  if (!instance) {
    // TODO(b/146809329): This case should probably destroy all trees.
    OnActionResult(data, false);
    return;
  }

  instance->PerformAction(
      std::move(action_data),
      base::BindOnce(&ArcAccessibilityHelperBridge::OnActionResult,
                     base::Unretained(this), data));
}

bool ArcAccessibilityHelperBridge::UseFullFocusMode() const {
  return use_full_focus_mode_;
}

void ArcAccessibilityHelperBridge::OnTaskDestroyed(int32_t task_id) {
  trees_.erase(KeyForTaskId(task_id));
  base::EraseIf(window_id_to_task_id_,
                [task_id](auto it) { return it.second == task_id; });
}

void ArcAccessibilityHelperBridge::OnAndroidVirtualKeyboardVisibilityChanged(
    bool visible) {
  if (!visible)
    trees_.erase(KeyForInputMethod());
}

void ArcAccessibilityHelperBridge::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  const std::string& notification_key = surface->GetNotificationKey();

  auto* const tree = GetFromKey(KeyForNotification(notification_key));
  if (!tree)
    return;

  surface->SetAXTreeId(tree->ax_tree_id());
  tree->set_device_scale_factor(
      DeviceScaleFactorFromWindow(surface->GetWindow()));

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

void ArcAccessibilityHelperBridge::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  UpdateWindowProperties(gained_active);

  // Transitioning with ARC and non-ARC window may need to dispatch
  // ToggleNativeChromeVoxArcSupport event.
  //  - When non-ChromeVox ARC window becomes inactive, dispatch |true|.
  //  - When non-ChromeVox ARC window becomes active, dispatch |false|.
  bool lost_arc = arc::IsArcAppWindow(lost_active);
  bool gained_arc = arc::IsArcAppWindow(gained_active);
  bool talkback_enabled = !native_chromevox_enabled_;
  if (talkback_enabled && lost_arc != gained_arc)
    DispatchCustomSpokenFeedbackToggled(gained_arc);

  if (lost_arc)
    lost_active->RemoveObserver(this);
  if (gained_arc) {
    UpdateWindowIdMapping(gained_active);
    gained_active->AddObserver(this);
  }
}

void ArcAccessibilityHelperBridge::OnWindowPropertyChanged(aura::Window* window,
                                                           const void* key,
                                                           intptr_t old) {
  // We are only interested in changes to |kClientAccessibilityIdKey|,
  // but that constant is not accessible outside shell_surface.cc.
  // So we react to all property changes.
  UpdateWindowIdMapping(window);
}

void ArcAccessibilityHelperBridge::InvokeUpdateEnabledFeatureForTesting() {
  UpdateEnabledFeature();
}

aura::Window* ArcAccessibilityHelperBridge::GetActiveWindow() {
  if (!exo::WMHelper::HasInstance())
    return nullptr;

  return exo::WMHelper::GetInstance()->GetActiveWindow();
}

extensions::EventRouter* ArcAccessibilityHelperBridge::GetEventRouter() const {
  return extensions::EventRouter::Get(profile_);
}

arc::mojom::AccessibilityFilterType
ArcAccessibilityHelperBridge::GetFilterTypeForProfile(Profile* profile) {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  const chromeos::MagnificationManager* magnification_manager =
      chromeos::MagnificationManager::Get();

  if (!accessibility_manager || !magnification_manager)
    return arc::mojom::AccessibilityFilterType::OFF;

  // TODO(yawano): Support the case where primary user is in background.
  if (accessibility_manager->profile() != profile)
    return arc::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled() ||
      accessibility_manager->IsSpokenFeedbackEnabled()) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  if (magnification_manager->IsMagnifierEnabled() ||
      magnification_manager->IsDockedMagnifierEnabled() ||
      accessibility_manager->IsFocusHighlightEnabled()) {
    return arc::mojom::AccessibilityFilterType::FOCUS;
  }

  return arc::mojom::AccessibilityFilterType::OFF;
}

void ArcAccessibilityHelperBridge::UpdateCaptionSettings() const {
  arc::mojom::CaptionStylePtr caption_style =
      GetCaptionStyleFromPrefs(profile_->GetPrefs());

  if (!caption_style)
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetCaptionStyle);

  if (!instance)
    return;

  instance->SetCaptionStyle(std::move(caption_style));
}

void ArcAccessibilityHelperBridge::OnActionResult(const ui::AXActionData& data,
                                                  bool result) const {
  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyActionResult(data, result);
}

void ArcAccessibilityHelperBridge::OnGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& result_rect) const {
  AXTreeSourceArc* tree_source = GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyGetTextLocationDataResult(
      data, OnGetTextLocationDataResultInternal(result_rect));
}

base::Optional<gfx::Rect>
ArcAccessibilityHelperBridge::OnGetTextLocationDataResultInternal(
    const base::Optional<gfx::Rect>& result_rect) const {
  if (!result_rect)
    return base::nullopt;

  DCHECK(exo::WMHelper::HasInstance());
  aura::Window* active_window = exo::WMHelper::GetInstance()->GetActiveWindow();
  if (!active_window)
    return base::nullopt;

  gfx::RectF rect_f = arc::ToChromeScale(*result_rect);
  rect_f.Scale(DeviceScaleFactorFromWindow(active_window));
  return gfx::ToEnclosingRect(rect_f);
}

void ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_FOCUS_HIGHLIGHT &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_SWITCH_ACCESS &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_DOCKED_MAGNIFIER &&
      event_details.notification_type !=
          chromeos::ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER) {
    return;
  }

  UpdateEnabledFeature();
  UpdateWindowProperties(GetActiveWindow());

  if (event_details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    SetExploreByTouchEnabled(event_details.enabled);
  }
}

void ArcAccessibilityHelperBridge::UpdateEnabledFeature() {
  arc::mojom::AccessibilityFilterType new_filter_type =
      GetFilterTypeForProfile(profile_);
  // Clear trees when filter type is changed to non-ALL.

  if (filter_type_ != new_filter_type &&
      new_filter_type != arc::mojom::AccessibilityFilterType::ALL) {
    trees_.clear();
  }
  filter_type_ = new_filter_type;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetFilter);
  if (instance)
    instance->SetFilter(filter_type_);

  const chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  const chromeos::MagnificationManager* magnification_manager =
      chromeos::MagnificationManager::Get();

  if (!accessibility_manager || !magnification_manager)
    return;

  is_focus_event_enabled_ = magnification_manager->IsMagnifierEnabled() ||
                            magnification_manager->IsDockedMagnifierEnabled() ||
                            accessibility_manager->IsFocusHighlightEnabled();

  use_full_focus_mode_ = accessibility_manager->IsSwitchAccessEnabled() ||
                         accessibility_manager->IsSpokenFeedbackEnabled();

  bool add_activation_observer =
      filter_type_ == arc::mojom::AccessibilityFilterType::ALL;
  if (add_activation_observer == activation_observer_added_)
    return;

  if (!exo::WMHelper::HasInstance())
    return;

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  aura::Window* active_window = GetActiveWindow();
  bool is_arc_active = arc::IsArcAppWindow(active_window);
  if (add_activation_observer) {
    wm_helper->AddActivationObserver(this);
    activation_observer_added_ = true;
    if (is_arc_active)
      active_window->AddObserver(this);
  } else {
    activation_observer_added_ = false;
    wm_helper->RemoveActivationObserver(this);
    if (is_arc_active)
      active_window->RemoveObserver(this);
  }
}

void ArcAccessibilityHelperBridge::UpdateWindowProperties(
    aura::Window* window) {
  if (!arc::IsArcAppWindow(window))
    return;

  int32_t task_id = arc::GetWindowTaskId(window);
  if (task_id == kNoTaskId)
    return;

  // Do a lookup for the tree source. A tree source may not exist because the
  // app isn't allowlisted Android side or no data has been received for the
  // app.
  bool use_talkback = talkback_enabled_task_ids_.count(task_id) > 0;

  window->SetProperty(aura::client::kAccessibilityTouchExplorationPassThrough,
                      use_talkback);
  window->SetProperty(ash::kSearchKeyAcceleratorReservedKey, use_talkback);
  window->SetProperty(aura::client::kAccessibilityFocusFallsbackToWidgetKey,
                      !use_talkback);

  if (use_talkback) {
    SetChildAxTreeIDForWindow(window, ui::AXTreeIDUnknown());
  } else if (GetFilterTypeForProfile(profile_) ==
             arc::mojom::AccessibilityFilterType::ALL) {
    TreeKey key = KeyForTaskId(task_id);
    AXTreeSourceArc* tree = GetFromKey(key);
    if (!tree)
      tree = CreateFromKey(std::move(key), window);

    // Just after the creation of window, widget has not been set yet and this
    // is not dispatched to ShellSurfaceBase. Thus, call this every time.
    SetChildAxTreeIDForWindow(window, tree->ax_tree_id());
  }
}

void ArcAccessibilityHelperBridge::SetExploreByTouchEnabled(bool enabled) {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetExploreByTouchEnabled);
  if (instance)
    instance->SetExploreByTouchEnabled(enabled);
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

void ArcAccessibilityHelperBridge::HandleFilterTypeFocusEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  if (event_data.get()->node_data.size() == 1 &&
      event_data->event_type ==
          arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    DispatchFocusChange(event_data.get()->node_data[0].get(), profile_);
}

void ArcAccessibilityHelperBridge::HandleFilterTypeAllEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  if (ShouldAnnounceEvent(event_data.get())) {
    DispatchEventTextAnnouncement(event_data.get());
    return;
  }

  if (event_data->node_data.empty())
    return;

  AXTreeSourceArc* tree_source = nullptr;
  bool is_notification_event = event_data->notification_key.has_value();
  if (is_notification_event) {
    const std::string& notification_key = event_data->notification_key.value();

    // This bridge must receive OnNotificationStateChanged call for the
    // notification_key before this receives an accessibility event for it.
    tree_source = GetFromKey(KeyForNotification(notification_key));
    DCHECK(tree_source);
  } else if (event_data->is_input_method_window) {
    exo::InputMethodSurface* input_method_surface =
        exo::InputMethodSurface::GetInputMethodSurface();

    if (!input_method_surface)
      return;

    if (!trees_.count(KeyForInputMethod())) {
      auto* tree = CreateFromKey(KeyForInputMethod(),
                                 input_method_surface->host_window());
      input_method_surface->SetChildAxTreeId(tree->ax_tree_id());
    }

    tree_source = GetFromKey(KeyForInputMethod());
  } else {
    aura::Window* active_window = GetActiveWindow();
    if (!active_window)
      return;

    auto task_id = arc::GetWindowTaskId(active_window);
    if (event_data->task_id != kNoTaskId) {
      // Event data has task ID. Check task ID.
      if (task_id != event_data->task_id)
        return;
    } else {
      // Event data does not have task ID. Get task ID from window ID instead.
      auto task_id_itr = window_id_to_task_id_.find(event_data->window_id);
      if (task_id_itr == window_id_to_task_id_.end() ||
          task_id != task_id_itr->second) {
        return;
      }
    }

    auto key = KeyForTaskId(task_id);
    tree_source = GetFromKey(key);

    if (!tree_source) {
      tree_source = CreateFromKey(key, active_window);
      SetChildAxTreeIDForWindow(active_window, tree_source->ax_tree_id());
      if (chromeos::AccessibilityManager::Get() &&
          chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
        // Record metrics only when SpokenFeedback is enabled in order to
        // compare this with TalkBack usage.
        base::UmaHistogramBoolean("Arc.AccessibilityWithTalkBack", false);
      }
    } else {
      tree_source->set_device_scale_factor(
          DeviceScaleFactorFromWindow(active_window));
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
      ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(event_data->notification_key.value());
      if (surface) {
        surface->GetAttachedHost()->NotifyAccessibilityEvent(
            ax::mojom::Event::kTextSelectionChanged, true);
      }
    }
  } else if (!is_notification_event) {
    UpdateWindowProperties(GetActiveWindow());
  }

  if (is_focus_event_enabled_ &&
      event_data->event_type ==
          arc::mojom::AccessibilityEventType::VIEW_FOCUSED) {
    for (size_t i = 0; i < event_data->node_data.size(); ++i) {
      if (event_data->node_data[i]->id == event_data->source_id) {
        DispatchFocusChange(event_data->node_data[i].get(), profile_);
        break;
      }
    }
  }
}

void ArcAccessibilityHelperBridge::UpdateWindowIdMapping(aura::Window* window) {
  const auto window_id = exo::GetShellClientAccessibilityId(window);
  if (!window_id.has_value())
    return;

  if (window_id_to_task_id_.find(window_id.value()) !=
      window_id_to_task_id_.end()) {
    // We already know this window ID.
    return;
  }

  const int32_t task_id = arc::GetWindowTaskId(window);
  if (task_id == kNoTaskId)
    return;

  window_id_to_task_id_[window_id.value()] = task_id;

  // The window ID is new to us. Request the entire tree.
  arc::mojom::AccessibilityWindowKeyPtr window_key =
      arc::mojom::AccessibilityWindowKey::New();
  window_key->set_window_id(window_id.value());

  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->accessibility_helper(),
                                  RequestSendAccessibilityTree);
  if (!instance)
    return;

  instance->RequestSendAccessibilityTree(std::move(window_key));
}

void ArcAccessibilityHelperBridge::DispatchEventTextAnnouncement(
    mojom::AccessibilityEventData* event_data) const {
  if (!event_data->event_text.has_value())
    return;

  std::unique_ptr<base::ListValue> event_args(
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          Create(*(event_data->event_text)));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ANNOUNCE_FOR_ACCESSIBILITY,
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          kEventName,
      std::move(event_args)));
  GetEventRouter()->BroadcastEvent(std::move(event));
}

void ArcAccessibilityHelperBridge::DispatchCustomSpokenFeedbackToggled(
    bool enabled) const {
  std::unique_ptr<base::ListValue> event_args(
      extensions::api::accessibility_private::OnCustomSpokenFeedbackToggled::
          Create(enabled));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::
          ACCESSIBILITY_PRIVATE_ON_CUSTOM_SPOKEN_FEEDBACK_TOGGLED,
      extensions::api::accessibility_private::OnCustomSpokenFeedbackToggled::
          kEventName,
      std::move(event_args)));
  GetEventRouter()->BroadcastEvent(std::move(event));
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::CreateFromKey(
    TreeKey key,
    aura::Window* window) {
  auto tree = std::make_unique<AXTreeSourceArc>(
      this, DeviceScaleFactorFromWindow(window));
  auto* tree_ptr = tree.get();
  trees_.insert(std::make_pair(std::move(key), std::move(tree)));
  return tree_ptr;
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromKey(const TreeKey& key) {
  auto tree_it = trees_.find(key);
  if (tree_it == trees_.end())
    return nullptr;

  return tree_it->second.get();
}

AXTreeSourceArc* ArcAccessibilityHelperBridge::GetFromTreeId(
    ui::AXTreeID tree_id) const {
  for (auto it = trees_.begin(); it != trees_.end(); ++it) {
    ui::AXTreeData tree_data;
    it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return it->second.get();
  }
  return nullptr;
}

}  // namespace arc
