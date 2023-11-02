// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
struct AXNodeData;
}

namespace arc {

namespace mojom {
class AccessibilityEventData;
}

class DrawerLayoutHandler : public AXTreeSourceArc::Hook {
 public:
  static absl::optional<
      std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
  CreateIfNecessary(AXTreeSourceArc* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  explicit DrawerLayoutHandler(const std::string& name) : name_(name) {}

  // AXTreeSourceArc::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceArc* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;

 private:
  const std::string name_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_DRAWER_LAYOUT_HANDLER_H_
