// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_HELPER_INSTANCE_REMOTE_PROXY_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_HELPER_INSTANCE_REMOTE_PROXY_H_

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "base/memory/raw_ptr.h"

namespace arc {

class ArcBridgeService;

// This class is responsible for forwarding incoming call to remote android
// AccessibilityService process via mojo.
class AccessibilityHelperInstanceRemoteProxy {
 public:
  explicit AccessibilityHelperInstanceRemoteProxy(
      ArcBridgeService* arc_bridge_service)
      : arc_bridge_service_(arc_bridge_service) {}
  ~AccessibilityHelperInstanceRemoteProxy() = default;

  AccessibilityHelperInstanceRemoteProxy(
      AccessibilityHelperInstanceRemoteProxy&&) = delete;
  AccessibilityHelperInstanceRemoteProxy& operator=(
      AccessibilityHelperInstanceRemoteProxy&&) = delete;

  bool SetFilter(mojom::AccessibilityFilterType filter_type) const;

  bool PerformAction(
      mojom::AccessibilityActionDataPtr action_data_ptr,
      mojom::AccessibilityHelperInstance::PerformActionCallback callback) const;

  bool SetNativeChromeVoxArcSupportForFocusedWindow(
      bool enabled,
      mojom::AccessibilityHelperInstance::
          SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) const;

  bool SetExploreByTouchEnabled(bool enabled) const;

  bool RefreshWithExtraData(
      mojom::AccessibilityActionDataPtr action_data_ptr,
      mojom::AccessibilityHelperInstance::RefreshWithExtraDataCallback callback)
      const;

  bool SetCaptionStyle(mojom::CaptionStylePtr style_ptr) const;

  bool RequestSendAccessibilityTree(
      mojom::AccessibilityWindowKeyPtr window_key_ptr) const;

 private:
  const raw_ptr<ArcBridgeService, ExperimentalAsh>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_HELPER_INSTANCE_REMOTE_PROXY_H_
