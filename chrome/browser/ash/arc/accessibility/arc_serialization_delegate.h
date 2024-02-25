// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_SERIALIZATION_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_SERIALIZATION_DELEGATE_H_

#include "services/accessibility/android/ax_tree_source_android.h"

namespace arc {
class ArcSerializationDelegate
    : public ax::android::AXTreeSourceAndroid::SerializationDelegate {
 public:
  void PopulateBounds(const ax::android::AccessibilityInfoDataWrapper& node,
                      ui::AXNodeData& out_data) const override;
};
}  // namespace arc
#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_SERIALIZATION_DELEGATE_H_
