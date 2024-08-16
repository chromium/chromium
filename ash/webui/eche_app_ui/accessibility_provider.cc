
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/accessibility_provider.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"

namespace {
using AXActionType = ax::android::mojom::AccessibilityActionType;
using AXBooleanProperty = ax::android::mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData =
    ax::android::mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData =
    ax::android::mojom::AccessibilityCollectionItemInfoData;
using AXEventData = ax::android::mojom::AccessibilityEventData;
using AXEventIntListProperty =
    ax::android::mojom::AccessibilityEventIntListProperty;
using AXEventIntProperty = ax::android::mojom::AccessibilityEventIntProperty;
using AXEventType = ax::android::mojom::AccessibilityEventType;
using AXIntListProperty = ax::android::mojom::AccessibilityIntListProperty;
using AXIntProperty = ax::android::mojom::AccessibilityIntProperty;
using AXNodeInfoData = ax::android::mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = ax::android::mojom::AccessibilityRangeInfoData;
using AXStringProperty = ax::android::mojom::AccessibilityStringProperty;
using AXWindowBooleanProperty =
    ax::android::mojom::AccessibilityWindowBooleanProperty;
using AXWindowInfoData = ax::android::mojom::AccessibilityWindowInfoData;
using AXWindowIntProperty = ax::android::mojom::AccessibilityWindowIntProperty;
using AXWindowIntListProperty =
    ax::android::mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty =
    ax::android::mojom::AccessibilityWindowStringProperty;
}  // namespace

namespace ash::eche_app {

AccessibilityProvider::AccessibilityProvider(
    std::unique_ptr<AccessibilityProviderProxy> proxy)
    : proxy_(std::move(proxy)) {
  // Register callback to know when accessibility should be enabled or disabled.
  proxy_->SetAccessibilityEnabledStateChangedCallback(base::BindRepeating(
      &AccessibilityProvider::OnAccessibilityEnabledStateChanged,
      weak_ptr_factory_.GetWeakPtr()));
  proxy_->SetExploreByTouchEnabledStateChangedCallback(base::BindRepeating(
      &AccessibilityProvider::OnExploreByTouchEnabledStateChanged,
      weak_ptr_factory_.GetWeakPtr()));
}
AccessibilityProvider::~AccessibilityProvider() = default;

void AccessibilityProvider::TrackView(AshWebView* view) {
  if (!base::FeatureList::IsEnabled(
          features::kEcheSWAProcessAndroidAccessibilityTree)) {
    // Don't track views if a11y tree is not enabled.
    return;
  }
  tree_source_ = std::make_unique<ax::android::AXTreeSourceAndroid>(
      this, std::make_unique<SerializationDelegate>(device_bounds_),
      view->GetNativeView() /*window*/);
  auto* child_view = view->GetViewByID(kAshWebViewChildWebViewId);
  CHECK(child_view);
  auto* webview = static_cast<views::WebView*>(child_view);
  webview->set_lock_child_ax_tree_id_override(true);
  webview->GetViewAccessibility().SetChildTreeID(
      tree_source_->ax_tree_id());
  auto* window_ptr =
      webview->web_contents()->GetRenderWidgetHostView()->GetNativeView();
  CHECK(window_ptr);
  // The render host view gets hit tests, set the hit test tree id.
  window_ptr->SetProperty(ui::kChildAXTreeID,
                          tree_source_->ax_tree_id().ToString());
  proxy_->OnViewTracked();
}

void AccessibilityProvider::HandleStreamClosed() {
  tree_source_.reset();
}

void AccessibilityProvider::HandleAccessibilityEventReceived(
    const std::vector<uint8_t>& serialized_proto) {
  if (!base::FeatureList::IsEnabled(
          features::kEcheSWAProcessAndroidAccessibilityTree)) {
    return;
  }

  if (serialized_proto.empty()) {
    return;
  }

  switch (GetFilterType()) {
    case ax::android::mojom::AccessibilityFilterType::ALL: {
      AccessibilityTreeConverter converter;
      // Parse the proto first so we can extract device screen size.
      proto::AccessibilityEventData proto_event_data;
      if (!converter.DeserializeProto(serialized_proto, &proto_event_data)) {
        return;
      }
      UpdateDeviceBounds(proto_event_data.display_info());
      auto mojom_event_data =
          converter.ConvertEventDataProtoToMojom(proto_event_data);
      if (mojom_event_data) {
        // Pass into correct tree. Currently there is only one.
        // Tree source will only be initialized once TrackView has been called.
        // Sometimes an event will come it right as the window is closed, which
        // can cause a crash with a check.
        if (tree_source_) {
          tree_source_->NotifyAccessibilityEvent(mojom_event_data.get());
        }
      }
    } break;
    case ax::android::mojom::AccessibilityFilterType::FOCUS:
      // TODO(francisjp): b/265817804
      break;
    case ax::android::mojom::AccessibilityFilterType::OFF:
      break;
    case ax::android::mojom::AccessibilityFilterType::INVALID_ENUM_VALUE:
      NOTREACHED();
  }
}

void AccessibilityProvider::SetAccessibilityObserver(
    mojo::PendingRemote<mojom::AccessibilityObserver> observer) {
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void AccessibilityProvider::IsAccessibilityEnabled(
    IsAccessibilityEnabledCallback callback) {
  std::move(callback).Run(proxy_->IsAccessibilityEnabled());
}

bool AccessibilityProvider::UseFullFocusMode() const {
  return proxy_->UseFullFocusMode();
}

ax::android::mojom::AccessibilityFilterType
AccessibilityProvider::GetFilterType() {
  return proxy_->GetFilterType();
}

void AccessibilityProvider::UpdateDeviceBounds(
    const proto::Rect& device_bounds) {
  const int height = device_bounds.bottom() - device_bounds.top();
  const int width = device_bounds.right() - device_bounds.left();
  CHECK(height > 0);
  CHECK(width > 0);
  device_bounds_.set_size({width, height});
}

// TODO(b/296326746) The current implementation is a workaround for Select to
// Speak, and a proper fix is pending hit test logic for android trees.
void AccessibilityProvider::HandleHitTest(const ui::AXActionData& data) const {
  // A hit test may come in just after a user closes the window.
  if (!tree_source_ || !tree_source_->root_id().has_value()) {
    return;
  }

  auto* automation_router = extensions::AutomationEventRouter::GetInstance();
  ui::AXEvent event;
  event.action_request_id = data.request_id;
  event.event_from_action = data.action;
  event.event_intents = {};
  event.id = tree_source_->root_id().value();
  event.event_type = data.hit_test_event_to_fire;
  automation_router->DispatchAccessibilityEvents(tree_source_->ax_tree_id(), {},
                                                 data.target_point, {event});
}

void AccessibilityProvider::OnGetTextLocationDataResult(
    const ui::AXActionData& action,
    const std::optional<std::vector<uint8_t>>& serialized_text_location) const {
  std::optional<gfx::Rect> result_rect;
  // There was a rect returned. Parse and validate it.
  if (serialized_text_location.has_value()) {
    proto::Rect proto_rect;
    // Parse the rect.
    if (!proto_rect.ParseFromArray(serialized_text_location->data(),
                                   serialized_text_location->size())) {
      // Failed to parse the response. Fail the action.
      OnActionResult(action, false);
      return;
    }
    // Validate the rect.
    if (proto_rect.bottom() > proto_rect.top() &&
        proto_rect.right() > proto_rect.left()) {
      result_rect = OnGetTextLocationDataResultInternal(proto_rect);
    } else {
      // Rect was invalid. Fail the action.
      OnActionResult(action, false);
      return;
    }
  }
  if (!tree_source_) {
    // Simple return, there is no way to notify a
    // tree source that doesn't exist.
    return;
  }

  tree_source_->NotifyGetTextLocationDataResult(action, result_rect);
}

gfx::Rect AccessibilityProvider::OnGetTextLocationDataResultInternal(
    proto::Rect proto_rect) const {
  // TODO(francisjp): Determine if there is additional processing required.
  int height = proto_rect.bottom() - proto_rect.top();
  int width = proto_rect.right() - proto_rect.left();
  return gfx::Rect(proto_rect.top(), proto_rect.left(), width, height);
}

void AccessibilityProvider::OnAction(const ui::AXActionData& action) const {
  if (!tree_source_) {
    return;
  }
  if (!tree_source_->window_id().has_value()) {
    OnActionResult(action, false);
    return;
  }

  if (action.action == ax::mojom::Action::kHitTest) {
    HandleHitTest(action);
    return;
  }

  AccessibilityTreeConverter converter;
  auto proto_action =
      converter.ConvertActionDataToProto(action, *tree_source_->window_id());
  if (proto_action.has_value()) {
    size_t nbytes = proto_action->ByteSizeLong();
    std::vector<uint8_t> serialized_proto(nbytes);
    proto_action->SerializeToArray(serialized_proto.data(), nbytes);

    if (action.action == ax::mojom::Action::kGetTextLocation) {
      mojom::AccessibilityObserver::RefreshWithExtraDataCallback cb =
          base::BindOnce(&AccessibilityProvider::OnGetTextLocationDataResult,
                         weak_ptr_factory_.GetWeakPtr(), action);
      observer_remote_->RefreshWithExtraData(serialized_proto, std::move(cb));
    } else {
      mojom::AccessibilityObserver::PerformActionCallback cb =
          base::BindOnce(&AccessibilityProvider::OnActionResult,
                         weak_ptr_factory_.GetWeakPtr(), action);
      observer_remote_->PerformAction(serialized_proto, std::move(cb));
    }

  } else {
    OnActionResult(action, false);
  }
}

void AccessibilityProvider::Bind(
    mojo::PendingReceiver<mojom::AccessibilityProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void AccessibilityProvider::OnActionResult(const ui::AXActionData& action,
                                           bool result) const {
  // Tree source could be null if the app is switched before the response comes
  // back.
  if (tree_source_) {
    tree_source_->NotifyActionResult(action, result);
  }
}

void AccessibilityProvider::OnAccessibilityEnabledStateChanged(bool enabled) {
  observer_remote_->EnableAccessibilityTreeStreaming(enabled);
}

void AccessibilityProvider::OnExploreByTouchEnabledStateChanged(bool enabled) {
  observer_remote_->EnableExploreByTouch(enabled);
}

// Serialization Delegate implementation

gfx::RectF
AccessibilityProvider::SerializationDelegate::ScaleAndroidPxToChromePx(
    const ax::android::AccessibilityInfoDataWrapper& node,
    aura::Window* window) const {
  gfx::Rect android_bounds = node.GetBounds();
  const auto* window_node = tree_source_->GetRoot();
  gfx::Rect window_res = window_node->GetWindow()->bounds_in_screen;
  const gfx::Rect chrome_window_bounds = window->GetBoundsInScreen();
  float x_scale =
      (float)chrome_window_bounds.width() / (float)device_bounds_->width();
  float y_scale =
      (float)chrome_window_bounds.height() / (float)device_bounds_->height();
  gfx::RectF chrome_bounds(android_bounds);
  const float chrome_dsf =
      window->GetToplevelWindow()->layer()->device_scale_factor();
  // Nodes need to be offset inside of their window.
  if (node.IsNode()) {
    chrome_bounds.Offset(-window_res.x(), -window_res.y());
  }
  chrome_bounds.Scale(x_scale * chrome_dsf, y_scale * chrome_dsf);
  return chrome_bounds;
}

void AccessibilityProvider::SerializationDelegate::PopulateBounds(
    const ax::android::AccessibilityInfoDataWrapper& node,
    ui::AXNodeData& out_data) const {
  gfx::RectF& out_bounds_px = out_data.relative_bounds.bounds;
  auto* window = tree_source_->window();
  out_bounds_px = ScaleAndroidPxToChromePx(node, window);
}

AccessibilityProvider::SerializationDelegate::SerializationDelegate(
    gfx::Rect& device_bounds)
    : device_bounds_(device_bounds) {}
}  // namespace ash::eche_app
