// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_accessibility_helper_instance.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace arc {

FakeAccessibilityHelperInstance::FakeAccessibilityHelperInstance() = default;
FakeAccessibilityHelperInstance::~FakeAccessibilityHelperInstance() = default;

void FakeAccessibilityHelperInstance::Init(
    mojo::PendingRemote<mojom::AccessibilityHelperHost> host_remote,
    InitCallback callback) {
  std::move(callback).Run();
}

void FakeAccessibilityHelperInstance::SetFilter(
    mojom::AccessibilityFilterType filter_type) {
  filter_type_ = filter_type;
}

void FakeAccessibilityHelperInstance::PerformAction(
    mojom::AccessibilityActionDataPtr action_data_ptr,
    PerformActionCallback callback) {
  last_requested_action_ = std::move(action_data_ptr);
  std::move(callback).Run(true);
}

void FakeAccessibilityHelperInstance::
    SetNativeChromeVoxArcSupportForFocusedWindowDeprecated(
        bool enabled,
        SetNativeChromeVoxArcSupportForFocusedWindowDeprecatedCallback
            callback) {
  std::move(callback).Run(true);
}

void FakeAccessibilityHelperInstance::
    SetNativeChromeVoxArcSupportForFocusedWindow(
        bool enabled,
        SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) {
  std::move(callback).Run(arc::mojom::SetNativeChromeVoxResponse::SUCCESS);
}

void FakeAccessibilityHelperInstance::SetExploreByTouchEnabled(bool enabled) {
  explore_by_touch_enabled_ = enabled;
}

void FakeAccessibilityHelperInstance::RefreshWithExtraData(
    mojom::AccessibilityActionDataPtr action_data_ptr,
    RefreshWithExtraDataCallback callback) {
  last_requested_action_ = std::move(action_data_ptr);
  refresh_with_extra_data_callback_ = std::move(callback);
}

void FakeAccessibilityHelperInstance::SetCaptionStyle(
    mojom::CaptionStylePtr style_ptr) {}

void FakeAccessibilityHelperInstance::RequestSendAccessibilityTree(
    mojom::AccessibilityWindowKeyPtr window_ptr) {
  last_requested_tree_window_key_ = std::move(window_ptr);
}

}  // namespace arc
