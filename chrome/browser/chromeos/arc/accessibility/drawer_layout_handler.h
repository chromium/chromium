// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/optional.h"
#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

namespace ui {
struct AXNodeData;
}

namespace arc {

namespace mojom {
class AccessibilityEventData;
}

class DrawerLayoutHandler : public AXTreeSourceArc::Hook {
 public:
  static base::Optional<
      std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
  CreateIfNecessary(AXTreeSourceArc* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  explicit DrawerLayoutHandler(const std::string& name) : name_(name) {}

  // AXTreeSourceArc::Hook overrides:
  void PostSerializeNode(ui::AXNodeData* out_data) const override;

 private:
  const std::string name_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_
