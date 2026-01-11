// Copyright 2026 The Chromium Authors
// Use of this code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_
#define CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"

namespace mcp_server {

// AccessibilitySnapshot provides LLM-friendly semantic snapshots of web pages
// using Chromium's accessibility tree API. Returns structured text format
// instead of raw HTML, enabling faster and more deterministic browser
// automation for AI agents.
//
// Example usage:
//   AccessibilitySnapshot::TakeSnapshot(
//       web_contents,
//       base::BindOnce(&MyHandler::OnSnapshotComplete, base::Unretained(this)));
//
class AccessibilitySnapshot {
 public:
  // Callback type for snapshot completion
  // Parameters: (success, error_message, snapshot_json)
  using SnapshotCallback =
      base::OnceCallback<void(bool, const std::string&, base::Value::Dict)>;

  AccessibilitySnapshot() = delete;
  ~AccessibilitySnapshot() = delete;

  // Takes an accessibility tree snapshot of the given WebContents
  // Calls callback with JSON containing:
  // - url: Page URL
  // - title: Page title
  // - tree: YAML-style text representation of accessibility tree
  //
  // Each element in the tree includes:
  // - role: Element role (button, textbox, link, etc.)
  // - name: Accessible name / label
  // - ref: Reference ID for subsequent interactions (e.g., [ref=e1])
  // - value: Current value (for inputs)
  // - state: Element state (enabled, checked, etc.)
  static void TakeSnapshot(content::WebContents* web_contents,
                           SnapshotCallback callback);

 private:
  // Processes the AXTreeUpdate and generates the snapshot
  static void ProcessSnapshot(SnapshotCallback callback,
                              std::string url,
                              std::string title,
                              ui::AXTreeUpdate& update);

  // Serializes AXTreeUpdate to YAML-style text format
  static std::string SerializeTree(const ui::AXTreeUpdate& update);

  // Determines if a node should be included in the snapshot
  // Filters out non-interactive and irrelevant elements
  static bool ShouldIncludeNode(const ui::AXNodeData& node);

  // Formats a single node as a YAML-style line
  // Returns string like: "- role: button\n  name: Click me\n  [ref=e5]"
  static std::string FormatNode(const ui::AXNodeData& node,
                                int indent_level,
                                const std::string& ref_id);

  // Gets accessible name from node data
  static std::string GetAccessibleName(const ui::AXNodeData& node);

  // Gets value from node data (for inputs, selects, etc.)
  static std::string GetNodeValue(const ui::AXNodeData& node);

  // Gets state information (enabled, checked, expanded, etc.)
  static std::string GetNodeState(const ui::AXNodeData& node);
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_
