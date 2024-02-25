// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/accessibility_helper_instance_remote_proxy.h"

#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_holder.h"

namespace arc {

bool AccessibilityHelperInstanceRemoteProxy::SetFilter(
    ax::android::mojom::AccessibilityFilterType filter_type) const {
  auto* const instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetFilter);
  if (!instance)
    return false;

  instance->SetFilter(filter_type);
  return true;
}

bool AccessibilityHelperInstanceRemoteProxy::PerformAction(
    ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
    ax::android::mojom::AccessibilityHelperInstance::PerformActionCallback
        callback) const {
  auto* const instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), PerformAction);
  if (!instance)
    return false;

  instance->PerformAction(std::move(action_data_ptr), std::move(callback));
  return true;
}

bool AccessibilityHelperInstanceRemoteProxy::
    SetNativeChromeVoxArcSupportForFocusedWindow(
        bool enabled,
        ax::android::mojom::AccessibilityHelperInstance::
            SetNativeChromeVoxArcSupportForFocusedWindowCallback callback)
        const {
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->accessibility_helper(),
                                  SetNativeChromeVoxArcSupportForFocusedWindow);
  if (!instance)
    return false;

  instance->SetNativeChromeVoxArcSupportForFocusedWindow(enabled,
                                                         std::move(callback));
  return true;
}

bool AccessibilityHelperInstanceRemoteProxy::SetExploreByTouchEnabled(
    bool enabled) const {
  auto* const instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), SetExploreByTouchEnabled);
  if (!instance)
    return false;

  instance->SetExploreByTouchEnabled(enabled);
  return true;
}

bool AccessibilityHelperInstanceRemoteProxy::RefreshWithExtraData(
    ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
    ax::android::mojom::AccessibilityHelperInstance::
        RefreshWithExtraDataCallback callback) const {
  auto* const instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->accessibility_helper(), RefreshWithExtraData);
  if (!instance)
    return false;

  instance->RefreshWithExtraData(std::move(action_data_ptr),
                                 std::move(callback));
  return true;
}

bool AccessibilityHelperInstanceRemoteProxy::RequestSendAccessibilityTree(
    ax::android::mojom::AccessibilityWindowKeyPtr window_key_ptr) const {
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->accessibility_helper(),
                                  RequestSendAccessibilityTree);
  if (!instance)
    return false;

  instance->RequestSendAccessibilityTree(std::move(window_key_ptr));
  return true;
}

}  // namespace arc
