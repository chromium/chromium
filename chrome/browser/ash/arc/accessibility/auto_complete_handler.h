// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
struct AXNodeData;
}

namespace arc {

namespace mojom {
class AccessibilityEventData;
}

class AutoCompleteHandler : public AXTreeSourceArc::Hook {
 public:
  using IdAndHandler = std::pair<int32_t, std::unique_ptr<AutoCompleteHandler>>;
  static std::vector<IdAndHandler> CreateIfNecessary(
      AXTreeSourceArc* tree_source,
      const mojom::AccessibilityEventData& event_data);

  explicit AutoCompleteHandler(const int32_t editable_node_id);

  ~AutoCompleteHandler() override;

  // AXTreeSourceArc::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceArc* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;

 private:
  const int32_t anchored_node_id_;
  absl::optional<int32_t> suggestion_window_id_;
  absl::optional<int32_t> selected_node_id_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_AUTO_COMPLETE_HANDLER_H_
