// Copyright 2026 The Chromium Authors
// Use of this code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_
#define CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
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

  // Gets a CSS selector for a given reference ID from the last snapshot
  // Returns empty string if ref ID not found
  // Format of selector attempts to uniquely identify the element using:
  // - Element ID if available
  // - Data attributes if available
  // - Position-based selector as fallback
  static std::string GetSelectorForRef(content::WebContents* web_contents,
                                       const std::string& ref_id);

 private:
  // Processes the AXTreeUpdate and generates the snapshot
  static void ProcessSnapshot(SnapshotCallback callback,
                              content::WebContents* web_contents,
                              std::string url,
                              std::string title,
                              ui::AXTreeUpdate& update);

  // Serializes AXTreeUpdate to YAML-style text format
  static std::string SerializeTree(const ui::AXTreeUpdate& update,
                                   content::WebContents* web_contents);

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

  // Stores the last snapshot's ref-to-node mapping per WebContents
  // Key: WebContents pointer, Value: Map of ref_id -> node data needed for selector generation
  struct NodeSelectorInfo {
    NodeSelectorInfo();
    ~NodeSelectorInfo();

    std::string element_id;      // HTML id attribute
    std::string role_str;        // Role as string
    std::string name;            // Accessible name
    int index_in_parent;         // Position among siblings
    std::vector<std::string> classes;  // HTML classes
  };
  static base::NoDestructor<std::map<content::WebContents*, std::map<std::string, NodeSelectorInfo>>> ref_mappings_;

  // Stores ref_id during tree serialization
  static void StoreRefMapping(content::WebContents* web_contents,
                              const std::string& ref_id,
                              const ui::AXNodeData& node,
                              int index_in_parent);
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_ACCESSIBILITY_SNAPSHOT_ACCESSIBILITY_SNAPSHOT_H_
