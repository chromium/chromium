// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_

#include <vector>

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"
#include "ui/accessibility/ax_node_data.h"

namespace arc {

class AXTreeSourceArc;

// Wrapper class for an AccessibilityWindowInfoData.
class AccessibilityWindowInfoDataWrapper : public AccessibilityInfoDataWrapper {
 public:
  AccessibilityWindowInfoDataWrapper(
      AXTreeSourceArc* tree_source,
      mojom::AccessibilityWindowInfoData* window);

  // AccessibilityInfoDataWrapper overrides.
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
  void GetChildren(
      std::vector<AccessibilityInfoDataWrapper*>* children) const override;

 private:
  bool GetProperty(mojom::AccessibilityWindowBooleanProperty prop) const;
  bool GetProperty(mojom::AccessibilityWindowIntProperty prop,
                   int32_t* out_value) const;
  bool HasProperty(mojom::AccessibilityWindowStringProperty prop) const;
  bool GetProperty(mojom::AccessibilityWindowStringProperty prop,
                   std::string* out_value) const;
  bool GetProperty(mojom::AccessibilityWindowIntListProperty prop,
                   std::vector<int32_t>* out_value) const;

  mojom::AccessibilityWindowInfoData* window_ptr_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityWindowInfoDataWrapper);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_
