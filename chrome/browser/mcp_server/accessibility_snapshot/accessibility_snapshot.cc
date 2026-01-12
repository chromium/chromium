// Copyright 2026 The Chromium Authors
// Use of this code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/accessibility_snapshot/accessibility_snapshot.h"

#include <sstream>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace mcp_server {

// Static member definition - wrapped in NoDestructor to avoid exit-time destructor
base::NoDestructor<std::map<content::WebContents*, std::map<std::string, AccessibilitySnapshot::NodeSelectorInfo>>>
    AccessibilitySnapshot::ref_mappings_;

// NodeSelectorInfo implementation
AccessibilitySnapshot::NodeSelectorInfo::NodeSelectorInfo() = default;
AccessibilitySnapshot::NodeSelectorInfo::~NodeSelectorInfo() = default;

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
                     std::move(callback), web_contents, std::move(url),
                     std::move(title)),
      ax_mode, kMaxNodes, kSnapshotTimeout,
      content::WebContents::AXTreeSnapshotPolicy::kAll);
}

// static
void AccessibilitySnapshot::ProcessSnapshot(SnapshotCallback callback,
                                            content::WebContents* web_contents,
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

  // Clear previous ref mappings for this WebContents
  (*ref_mappings_)[web_contents].clear();

  // Serialize tree to YAML-style format and store ref mappings
  std::string tree_string = SerializeTree(update, web_contents);

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
    const ui::AXTreeUpdate& update,
    content::WebContents* web_contents) {
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
  std::function<void(const ui::AXNodeData*, int, int)> walk_tree =
      [&](const ui::AXNodeData* node, int depth, int index_in_parent) {
        if (!node) {
          return;
        }

        // Check if this node should be included in output
        bool include_node = ShouldIncludeNode(*node);

        // If including this node, output it with a reference ID
        if (include_node) {
          // Generate reference ID for interactive elements (not for static text/paragraphs)
          std::string ref_id;
          if (node->role != ax::mojom::Role::kStaticText &&
              node->role != ax::mojom::Role::kParagraph) {
            ref_id = "e" + base::NumberToString(ref_counter++);

            // Store ref mapping for later use
            if (web_contents && !ref_id.empty()) {
              StoreRefMapping(web_contents, ref_id, *node, index_in_parent);
            }
          }

          // Format and output node
          std::string node_output = FormatNode(*node, depth, ref_id);
          if (!node_output.empty()) {
            output << node_output;
          }
        }

        // ALWAYS process children, even if we filtered out this node
        // This ensures we don't lose descendants of filtered containers
        // Only increment depth if we output this node
        int child_depth = include_node ? depth + 1 : depth;
        int child_index = 0;
        for (int32_t child_id : node->child_ids) {
          auto it = node_map.find(child_id);
          if (it != node_map.end()) {
            walk_tree(it->second, child_depth, child_index++);
          }
        }
      };

  walk_tree(root, 0, 0);

  return output.str();
}

// static
bool AccessibilitySnapshot::ShouldIncludeNode(const ui::AXNodeData& node) {
  // Skip invisible nodes first (they can't be interacted with)
  if (node.IsInvisible()) {
    return false;
  }

  // Filter by role - but be much more inclusive now
  if (!IsRelevantRole(node.role)) {
    return false;
  }

  // ALWAYS include ALL interactive elements - they're critical for automation
  // Include even without names - LLMs can identify by position/context
  if (node.role == ax::mojom::Role::kButton ||
      node.role == ax::mojom::Role::kCheckBox ||
      node.role == ax::mojom::Role::kRadioButton ||
      node.role == ax::mojom::Role::kTextField ||
      node.role == ax::mojom::Role::kTextFieldWithComboBox ||
      node.role == ax::mojom::Role::kSearchBox ||
      node.role == ax::mojom::Role::kLink ||
      node.role == ax::mojom::Role::kMenuItem ||
      node.role == ax::mojom::Role::kMenuItemCheckBox ||
      node.role == ax::mojom::Role::kMenuItemRadio ||
      node.role == ax::mojom::Role::kListBox ||
      node.role == ax::mojom::Role::kListBoxOption ||
      node.role == ax::mojom::Role::kComboBoxSelect ||
      node.role == ax::mojom::Role::kSlider ||
      node.role == ax::mojom::Role::kSpinButton ||
      node.role == ax::mojom::Role::kTab ||
      node.role == ax::mojom::Role::kToggleButton ||
      node.role == ax::mojom::Role::kSwitch) {
    return true;
  }

  // ALWAYS include structural/landmark elements - they provide context
  if (node.role == ax::mojom::Role::kRootWebArea ||
      node.role == ax::mojom::Role::kDocument ||
      node.role == ax::mojom::Role::kMain ||
      node.role == ax::mojom::Role::kNavigation ||
      node.role == ax::mojom::Role::kRegion ||
      node.role == ax::mojom::Role::kBanner ||
      node.role == ax::mojom::Role::kContentInfo ||
      node.role == ax::mojom::Role::kForm ||
      node.role == ax::mojom::Role::kSearch) {
    return true;
  }

  // ALWAYS include headings and images - important page structure
  if (node.role == ax::mojom::Role::kHeading ||
      node.role == ax::mojom::Role::kImage) {
    return true;
  }

  // For content elements, be more lenient - include if they have content OR children
  if (node.role == ax::mojom::Role::kStaticText ||
      node.role == ax::mojom::Role::kParagraph ||
      node.role == ax::mojom::Role::kList ||
      node.role == ax::mojom::Role::kListItem ||
      node.role == ax::mojom::Role::kArticle ||
      node.role == ax::mojom::Role::kSection) {
    // Include if has any text content
    std::string name = GetAccessibleName(node);
    if (!name.empty()) {
      return true;
    }
    // Include if has value
    std::string value = node.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    if (!value.empty()) {
      return true;
    }
    // Include if has children (container for content)
    if (!node.child_ids.empty()) {
      return true;
    }
    return false;
  }

  // For generic containers, always include if they have children
  // This ensures we don't lose important nested elements
  if (node.role == ax::mojom::Role::kGenericContainer) {
    return !node.child_ids.empty();
  }

  // Default: include the node if it passed the role filter
  // This makes us more inclusive by default
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
  std::string name;

  // Try multiple name sources in order of preference

  // 1. Accessible name (aria-label, aria-labelledby, or explicit label)
  name = node.GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (!name.empty()) {
    base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      if (name.length() > 200) {
        name = name.substr(0, 197) + "...";
      }
      return name;
    }
  }

  // 2. Placeholder text (for inputs)
  name = node.GetStringAttribute(ax::mojom::StringAttribute::kPlaceholder);
  if (!name.empty()) {
    base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      if (name.length() > 200) {
        name = name.substr(0, 197) + "...";
      }
      return name;
    }
  }

  // 3. Description (aria-describedby)
  name = node.GetStringAttribute(ax::mojom::StringAttribute::kDescription);
  if (!name.empty()) {
    base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      if (name.length() > 200) {
        name = name.substr(0, 197) + "...";
      }
      return name;
    }
  }

  // 4. Title attribute (HTML title)
  name = node.GetStringAttribute(ax::mojom::StringAttribute::kTooltip);
  if (!name.empty()) {
    base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
    if (!name.empty()) {
      if (name.length() > 200) {
        name = name.substr(0, 197) + "...";
      }
      return name;
    }
  }

  // 5. For text inputs, use the value as name if nothing else available
  if (node.role == ax::mojom::Role::kTextField ||
      node.role == ax::mojom::Role::kSearchBox ||
      node.role == ax::mojom::Role::kTextFieldWithComboBox) {
    name = node.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    if (!name.empty()) {
      base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
      if (!name.empty()) {
        if (name.length() > 200) {
          name = name.substr(0, 197) + "...";
        }
        return name;
      }
    }
  }

  return "";
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

// static
void AccessibilitySnapshot::StoreRefMapping(content::WebContents* web_contents,
                                            const std::string& ref_id,
                                            const ui::AXNodeData& node,
                                            int index_in_parent) {
  NodeSelectorInfo info;

  // Store HTML ID if available
  info.element_id = node.GetStringAttribute(ax::mojom::StringAttribute::kHtmlId);

  // Store role
  info.role_str = RoleToString(node.role);

  // Store accessible name
  info.name = GetAccessibleName(node);

  // Store index
  info.index_in_parent = index_in_parent;

  // Store HTML classes if available
  std::string class_str = node.GetStringAttribute(ax::mojom::StringAttribute::kClassName);
  if (!class_str.empty()) {
    info.classes = base::SplitString(class_str, " ", base::TRIM_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
  }

  (*ref_mappings_)[web_contents][ref_id] = info;
}

// static
std::string AccessibilitySnapshot::GetSelectorForRef(
    content::WebContents* web_contents,
    const std::string& ref_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if we have mappings for this WebContents
  auto wc_it = (*ref_mappings_).find(web_contents);
  if (wc_it == (*ref_mappings_).end()) {
    return "";
  }

  // Check if we have this ref_id
  auto ref_it = wc_it->second.find(ref_id);
  if (ref_it == wc_it->second.end()) {
    return "";
  }

  const NodeSelectorInfo& info = ref_it->second;

  // Build selector - try most specific first

  // 1. If element has ID, use that (most specific)
  if (!info.element_id.empty()) {
    return "#" + info.element_id;
  }

  // 2. Try role + name combination (fairly specific)
  if (!info.name.empty()) {
    // Convert role to element type selector for common cases
    std::string element_selector;
    if (info.role_str == "button") {
      element_selector = "button";
    } else if (info.role_str == "textbox") {
      element_selector = "input";
    } else if (info.role_str == "link") {
      element_selector = "a";
    } else if (info.role_str == "checkbox") {
      element_selector = "input[type='checkbox']";
    } else if (info.role_str == "radio") {
      element_selector = "input[type='radio']";
    } else if (info.role_str == "combobox") {
      element_selector = "select";
    } else {
      // Fallback to role as attribute selector
      element_selector = "[role='" + info.role_str + "']";
    }

    // Escape quotes in name for use in selector
    std::string escaped_name = info.name;
    base::ReplaceChars(escaped_name, "'", "\\'", &escaped_name);

    // Try aria-label selector
    return element_selector + "[aria-label='" + escaped_name + "']";
  }

  // 3. Try role + index (less specific, but works)
  if (info.index_in_parent >= 0) {
    std::string element_selector;
    if (info.role_str == "button") {
      element_selector = "button";
    } else if (info.role_str == "textbox") {
      element_selector = "input";
    } else if (info.role_str == "link") {
      element_selector = "a";
    } else {
      element_selector = "[role='" + info.role_str + "']";
    }

    return element_selector + ":nth-of-type(" +
           base::NumberToString(info.index_in_parent + 1) + ")";
  }

  // 4. Fallback: just use role (least specific)
  if (info.role_str == "button") {
    return "button";
  } else if (info.role_str == "textbox") {
    return "input";
  } else if (info.role_str == "link") {
    return "a";
  } else {
    return "[role='" + info.role_str + "']";
  }
}

}  // namespace mcp_server
