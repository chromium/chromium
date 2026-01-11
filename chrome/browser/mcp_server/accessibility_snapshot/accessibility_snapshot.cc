// Copyright 2026 The Chromium Authors
// Use of this code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/accessibility_snapshot/accessibility_snapshot.h"

#include <sstream>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace mcp_server {

namespace {

// Maximum number of nodes to include in snapshot
constexpr size_t kMaxNodes = 5000;

// Timeout for snapshot request
constexpr base::TimeDelta kSnapshotTimeout = base::Seconds(10);

// Indentation for YAML-style output
constexpr int kIndentSize = 2;

// Determines if a role is interactive/relevant for LLM automation
bool IsRelevantRole(ax::mojom::Role role) {
  switch (role) {
    // Interactive elements
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kComboBoxSelect:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kSwitch:

    // Structural elements
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kSearch:

    // Content elements
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kStaticText:
    case ax::mojom::Role::kParagraph:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kSection:
      return true;

    default:
      return false;
  }
}

// Converts role enum to human-readable string
std::string RoleToString(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
      return "button";
    case ax::mojom::Role::kCheckBox:
      return "checkbox";
    case ax::mojom::Role::kRadioButton:
      return "radio";
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      return "textbox";
    case ax::mojom::Role::kTextFieldWithComboBox:
      return "combobox";
    case ax::mojom::Role::kLink:
      return "link";
    case ax::mojom::Role::kMenuItem:
      return "menuitem";
    case ax::mojom::Role::kListBox:
      return "listbox";
    case ax::mojom::Role::kListBoxOption:
      return "option";
    case ax::mojom::Role::kComboBoxSelect:
      return "combobox";
    case ax::mojom::Role::kSlider:
      return "slider";
    case ax::mojom::Role::kHeading:
      return "heading";
    case ax::mojom::Role::kImage:
      return "image";
    case ax::mojom::Role::kStaticText:
      return "text";
    case ax::mojom::Role::kParagraph:
      return "paragraph";
    case ax::mojom::Role::kList:
      return "list";
    case ax::mojom::Role::kListItem:
      return "listitem";
    case ax::mojom::Role::kRootWebArea:
      return "webarea";
    case ax::mojom::Role::kDocument:
      return "document";
    case ax::mojom::Role::kGenericContainer:
      return "container";
    default:
      return "generic";
  }
}

}  // namespace

// static
void AccessibilitySnapshot::TakeSnapshot(content::WebContents* web_contents,
                                          SnapshotCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    std::move(callback).Run(false, "Invalid WebContents", base::Value::Dict());
    return;
  }

  // Get page metadata
  std::string url = web_contents->GetURL().spec();
  std::string title = base::UTF16ToUTF8(web_contents->GetTitle());

  // Request accessibility tree snapshot
  // Use kWebContents mode which includes form controls, HTML, CSS, and layout
  ui::AXMode ax_mode = ui::kAXModeWebContentsOnly;

  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AccessibilitySnapshot::ProcessSnapshot,
                     std::move(callback), std::move(url), std::move(title)),
      ax_mode, kMaxNodes, kSnapshotTimeout,
      content::WebContents::AXTreeSnapshotPolicy::kAll);
}

// static
void AccessibilitySnapshot::ProcessSnapshot(SnapshotCallback callback,
                                            std::string url,
                                            std::string title,
                                            ui::AXTreeUpdate& update) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if snapshot was successful
  if (update.nodes.empty()) {
    std::move(callback).Run(false, "Empty accessibility tree",
                            base::Value::Dict());
    return;
  }

  // Serialize tree to YAML-style format
  std::string tree_string = SerializeTree(update);

  // Build JSON response
  base::Value::Dict response;
  response.Set("url", url);
  response.Set("title", title);
  response.Set("tree", tree_string);
  response.Set("node_count", static_cast<int>(update.nodes.size()));

  std::move(callback).Run(true, "", std::move(response));
}

// static
std::string AccessibilitySnapshot::SerializeTree(
    const ui::AXTreeUpdate& update) {
  std::ostringstream output;

  // Create a map of node IDs to nodes for quick lookup
  std::unordered_map<int32_t, const ui::AXNodeData*> node_map;
  for (const auto& node : update.nodes) {
    node_map[node.id] = &node;
  }

  // Find root node
  const ui::AXNodeData* root = nullptr;
  if (update.root_id != ui::kInvalidAXNodeID) {
    // Use root_id if available
    auto it = node_map.find(update.root_id);
    if (it != node_map.end()) {
      root = it->second;
    }
  }

  // Fallback: use first node as root
  if (!root && !update.nodes.empty()) {
    root = &update.nodes[0];
  }

  if (!root) {
    return "# No accessibility tree available\n";
  }

  // Reference ID counter
  int ref_counter = 1;

  // Walk tree and generate output
  std::function<void(const ui::AXNodeData*, int)> walk_tree =
      [&](const ui::AXNodeData* node, int depth) {
        if (!node || !ShouldIncludeNode(*node)) {
          return;
        }

        // Generate reference ID for interactive elements
        std::string ref_id;
        if (node->role != ax::mojom::Role::kStaticText &&
            node->role != ax::mojom::Role::kParagraph) {
          ref_id = "e" + base::NumberToString(ref_counter++);
        }

        // Format and output node
        std::string node_output = FormatNode(*node, depth, ref_id);
        if (!node_output.empty()) {
          output << node_output;
        }

        // Process children
        for (int32_t child_id : node->child_ids) {
          auto it = node_map.find(child_id);
          if (it != node_map.end()) {
            walk_tree(it->second, depth + 1);
          }
        }
      };

  walk_tree(root, 0);

  return output.str();
}

// static
bool AccessibilitySnapshot::ShouldIncludeNode(const ui::AXNodeData& node) {
  // Filter by role
  if (!IsRelevantRole(node.role)) {
    return false;
  }

  // Skip invisible nodes
  if (node.IsInvisible()) {
    return false;
  }

  // Allow structural elements and static text even without explicit names
  // They often convey important page structure and content
  if (node.role == ax::mojom::Role::kMain ||
      node.role == ax::mojom::Role::kRegion ||
      node.role == ax::mojom::Role::kForm ||
      node.role == ax::mojom::Role::kStaticText ||
      node.role == ax::mojom::Role::kParagraph ||
      node.role == ax::mojom::Role::kHeading ||
      node.role == ax::mojom::Role::kSection ||
      node.role == ax::mojom::Role::kArticle) {
    return true;
  }

  // For interactive elements, require a name
  std::string name = GetAccessibleName(node);
  if (name.empty()) {
    return false;
  }

  return true;
}

// static
std::string AccessibilitySnapshot::FormatNode(const ui::AXNodeData& node,
                                              int indent_level,
                                              const std::string& ref_id) {
  std::ostringstream output;
  std::string indent(indent_level * kIndentSize, ' ');

  // Start with role
  output << indent << "- role: \"" << RoleToString(node.role) << "\"\n";

  // Add name if present
  std::string name = GetAccessibleName(node);
  if (!name.empty()) {
    output << indent << "  name: \"" << name << "\"\n";
  }

  // Add value if present (for inputs, etc.)
  std::string value = GetNodeValue(node);
  if (!value.empty()) {
    output << indent << "  value: \"" << value << "\"\n";
  }

  // Add heading level for headings
  if (node.role == ax::mojom::Role::kHeading) {
    auto level = node.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    if (level > 0) {
      output << indent << "  level: " << level << "\n";
    }
  }

  // Add state information
  std::string state = GetNodeState(node);
  if (!state.empty()) {
    output << indent << "  state: \"" << state << "\"\n";
  }

  // Add reference ID for interactive elements
  if (!ref_id.empty()) {
    output << indent << "  [ref=" << ref_id << "]\n";
  }

  return output.str();
}

// static
std::string AccessibilitySnapshot::GetAccessibleName(
    const ui::AXNodeData& node) {
  // Get name attribute (accessible name)
  std::string name =
      node.GetStringAttribute(ax::mojom::StringAttribute::kName);

  // Trim whitespace and limit length
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
  if (name.length() > 200) {
    name = name.substr(0, 197) + "...";
  }

  return name;
}

// static
std::string AccessibilitySnapshot::GetNodeValue(const ui::AXNodeData& node) {
  // Get value attribute (for inputs, textareas, selects, etc.)
  std::string value =
      node.GetStringAttribute(ax::mojom::StringAttribute::kValue);

  // Trim and limit length
  base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
  if (value.length() > 200) {
    value = value.substr(0, 197) + "...";
  }

  return value;
}

// static
std::string AccessibilitySnapshot::GetNodeState(const ui::AXNodeData& node) {
  std::vector<std::string> states;

  // Check restriction (disabled/read-only)
  auto restriction = node.GetRestriction();
  if (restriction == ax::mojom::Restriction::kDisabled) {
    states.push_back("disabled");
  } else if (restriction == ax::mojom::Restriction::kReadOnly) {
    states.push_back("readonly");
  } else {
    states.push_back("enabled");
  }

  // Check focusable state
  if (node.HasState(ax::mojom::State::kFocusable)) {
    states.push_back("focusable");
  }

  // Check expanded/collapsed
  if (node.HasState(ax::mojom::State::kExpanded)) {
    states.push_back("expanded");
  } else if (node.HasState(ax::mojom::State::kCollapsed)) {
    states.push_back("collapsed");
  }

  // Check checked state for checkboxes/radios
  auto checked_state = node.GetCheckedState();
  switch (checked_state) {
    case ax::mojom::CheckedState::kTrue:
      states.push_back("checked");
      break;
    case ax::mojom::CheckedState::kFalse:
      states.push_back("unchecked");
      break;
    case ax::mojom::CheckedState::kMixed:
      states.push_back("mixed");
      break;
    default:
      break;
  }

  // Join states with commas
  return base::JoinString(states, ", ");
}

}  // namespace mcp_server
