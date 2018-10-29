// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_info_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node_data.h"

namespace arc {

class AXTreeSourceArc;

// Wrapper class for an AccessibilityWindowInfoData.
class AccessibilityNodeInfoDataWrapper : public ArcAccessibilityInfoData {
 public:
  explicit AccessibilityNodeInfoDataWrapper(
      AXTreeSourceArc* tree_source,
      mojom::AccessibilityNodeInfoData* node);

  // ArcAccessibilityInfoData overrides.
  bool IsNode() const override;
  mojom::AccessibilityNodeInfoData* GetNode() const override;
  mojom::AccessibilityWindowInfoData* GetWindow() const override;
  int32_t GetId() const override;
  const gfx::Rect GetBounds() const override;
  bool IsVisibleToUser() const override;
  bool IsFocused() const override;
  bool CanBeAccessibilityFocused() const override;
  void PopulateAXRole(ui::AXNodeData* out_data) const override;
  void PopulateAXState(ui::AXNodeData* out_data) const override;
  void Serialize(ui::AXNodeData* out_data) const override;
  const std::vector<int32_t>* GetChildren() const override;

  mojom::AccessibilityNodeInfoData* node() { return node_ptr_; }

 private:
  bool GetProperty(mojom::AccessibilityBooleanProperty prop) const;
  bool GetProperty(mojom::AccessibilityIntProperty prop,
                   int32_t* out_value) const;
  bool HasProperty(mojom::AccessibilityStringProperty prop) const;
  bool GetProperty(mojom::AccessibilityStringProperty prop,
                   std::string* out_value) const;
  bool GetProperty(mojom::AccessibilityIntListProperty prop,
                   std::vector<int32_t>* out_value) const;
  bool GetProperty(mojom::AccessibilityStringListProperty prop,
                   std::vector<std::string>* out_value) const;

  bool HasCoveringSpan(mojom::AccessibilityStringProperty prop,
                       mojom::SpanType span_type) const;

  AXTreeSourceArc* tree_source_ = nullptr;
  mojom::AccessibilityNodeInfoData* node_ptr_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityNodeInfoDataWrapper);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
