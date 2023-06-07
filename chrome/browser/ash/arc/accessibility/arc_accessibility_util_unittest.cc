// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_test_util.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace arc {

using AXEventType = mojom::AccessibilityEventType;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;

TEST(ArcAccessibilityUtilTest, FromContentChangeTypesToAXEvent) {
  auto node_info_data = AXNodeInfoData::New();
  AccessibilityNodeInfoDataWrapper source_node_info_wrapper(
      nullptr, node_info_data.get());

  std::vector<int32_t> empty_list = {};
  EXPECT_EQ(absl::nullopt, FromContentChangeTypesToAXEvent(empty_list));

  std::vector<int32_t> state_description = {
      static_cast<int32_t>(mojom::ContentChangeType::STATE_DESCRIPTION)};
  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            FromContentChangeTypesToAXEvent(state_description));

  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            ToAXEvent(AXEventType::WINDOW_STATE_CHANGED, state_description,
                      &source_node_info_wrapper, &source_node_info_wrapper));
  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED, state_description,
                      &source_node_info_wrapper, &source_node_info_wrapper));

  std::vector<int32_t> without_state_description = {
      static_cast<int32_t>(mojom::ContentChangeType::TEXT)};
  EXPECT_EQ(absl::nullopt,
            FromContentChangeTypesToAXEvent(without_state_description));

  std::vector<int32_t> include_state_description = {
      static_cast<int32_t>(mojom::ContentChangeType::TEXT),
      static_cast<int32_t>(mojom::ContentChangeType::STATE_DESCRIPTION)};
  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            FromContentChangeTypesToAXEvent(include_state_description));

  EXPECT_EQ(
      ax::mojom::Event::kAriaAttributeChanged,
      ToAXEvent(AXEventType::WINDOW_STATE_CHANGED, include_state_description,
                &source_node_info_wrapper, &source_node_info_wrapper));
  EXPECT_EQ(
      ax::mojom::Event::kAriaAttributeChanged,
      ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED, include_state_description,
                &source_node_info_wrapper, &source_node_info_wrapper));

  std::vector<int32_t> not_enum_value = {111};
  EXPECT_EQ(absl::nullopt, FromContentChangeTypesToAXEvent(not_enum_value));
}

TEST(ArcAccessibilityUtilTest, LiveRegionChangeEvent) {
  auto node_info_data = AXNodeInfoData::New();

  std::vector<int32_t> empty_list = {};

  SetProperty(node_info_data.get(), AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::POLITE));

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper(
      nullptr, node_info_data.get());
  EXPECT_EQ(ax::mojom::Event::kLiveRegionChanged,
            ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED, empty_list,
                      &source_node_info_wrapper, &source_node_info_wrapper));

  // Check kLayoutComplete is returned for nodes with live region type NONE.
  SetProperty(node_info_data.get(), AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::NONE));

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper_none(
      nullptr, node_info_data.get());
  EXPECT_EQ(
      ax::mojom::Event::kLayoutComplete,
      ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED, empty_list,
                &source_node_info_wrapper, &source_node_info_wrapper_none));
}

}  // namespace arc
