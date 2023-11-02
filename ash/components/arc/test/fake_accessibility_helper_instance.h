// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

class FakeAccessibilityHelperInstance
    : public mojom::AccessibilityHelperInstance {
 public:
  FakeAccessibilityHelperInstance();

  FakeAccessibilityHelperInstance(const FakeAccessibilityHelperInstance&) =
      delete;
  FakeAccessibilityHelperInstance& operator=(
      const FakeAccessibilityHelperInstance&) = delete;

  ~FakeAccessibilityHelperInstance() override;

  void Init(mojo::PendingRemote<mojom::AccessibilityHelperHost> host_remote,
            InitCallback callback) override;
  void SetFilter(mojom::AccessibilityFilterType filter_type) override;
  void PerformAction(mojom::AccessibilityActionDataPtr action_data_ptr,
                     PerformActionCallback callback) override;
  void SetNativeChromeVoxArcSupportForFocusedWindowDeprecated(
      bool enabled,
      SetNativeChromeVoxArcSupportForFocusedWindowDeprecatedCallback callback)
      override;
  void SetNativeChromeVoxArcSupportForFocusedWindow(
      bool enabled,
      SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) override;
  void SetExploreByTouchEnabled(bool enabled) override;
  void RefreshWithExtraData(mojom::AccessibilityActionDataPtr action_data_ptr,
                            RefreshWithExtraDataCallback callback) override;
  void SetCaptionStyle(mojom::CaptionStylePtr style_ptr) override;
  void RequestSendAccessibilityTree(
      mojom::AccessibilityWindowKeyPtr window_ptr) override;

  mojom::AccessibilityFilterType filter_type() { return filter_type_; }
  bool explore_by_touch_enabled() { return explore_by_touch_enabled_; }
  mojom::AccessibilityActionData* last_requested_action() {
    return last_requested_action_.get();
  }
  mojom::AccessibilityWindowKey* last_requested_tree_window_key() {
    return last_requested_tree_window_key_.get();
  }
  RefreshWithExtraDataCallback refresh_with_extra_data_callback() {
    return std::move(refresh_with_extra_data_callback_);
  }

 private:
  mojom::AccessibilityFilterType filter_type_ =
      mojom::AccessibilityFilterType::OFF;

  // Explore-by-touch is enabled by default in ARC++, so we default it to 'true'
  // in this test as well.
  bool explore_by_touch_enabled_ = true;

  mojom::AccessibilityActionDataPtr last_requested_action_;
  mojom::AccessibilityWindowKeyPtr last_requested_tree_window_key_;
  RefreshWithExtraDataCallback refresh_with_extra_data_callback_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
