// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "components/exo/wm_helper.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;
using AXStringListProperty = mojom::AccessibilityStringListProperty;
using AXStringProperty = mojom::AccessibilityStringProperty;

AccessibilityNodeInfoDataWrapper::AccessibilityNodeInfoDataWrapper(
    AXTreeSourceArc* tree_source,
    AXNodeInfoData* node)
    : tree_source_(tree_source), node_ptr_(node) {}

bool AccessibilityNodeInfoDataWrapper::IsNode() const {
  return true;
}

mojom::AccessibilityNodeInfoData* AccessibilityNodeInfoDataWrapper::GetNode()
    const {
  return node_ptr_;
}

mojom::AccessibilityWindowInfoData*
AccessibilityNodeInfoDataWrapper::GetWindow() const {
  return nullptr;
}

int32_t AccessibilityNodeInfoDataWrapper::GetId() const {
  return node_ptr_->id;
}

const gfx::Rect AccessibilityNodeInfoDataWrapper::GetBounds() const {
  return node_ptr_->bounds_in_screen;
}

bool AccessibilityNodeInfoDataWrapper::IsVisibleToUser() const {
  return GetProperty(AXBooleanProperty::VISIBLE_TO_USER);
}

bool AccessibilityNodeInfoDataWrapper::IsFocused() const {
  return GetProperty(AXBooleanProperty::FOCUSED);
}

bool AccessibilityNodeInfoDataWrapper::CanBeAccessibilityFocused() const {
  // In Chrome, this means:
  // a node with a non-generic role and:
  // actionable nodes or top level scrollables with a name
  ui::AXNodeData data;
  PopulateAXRole(&data);
  bool non_generic_role = data.role != ax::mojom::Role::kGenericContainer &&
                          data.role != ax::mojom::Role::kGroup;
  bool actionable = GetProperty(AXBooleanProperty::CLICKABLE) ||
                    GetProperty(AXBooleanProperty::FOCUSABLE) ||
                    GetProperty(AXBooleanProperty::CHECKABLE);
  bool top_level_scrollable = HasProperty(AXStringProperty::TEXT) &&
                              GetProperty(AXBooleanProperty::SCROLLABLE);
  return non_generic_role && (actionable || top_level_scrollable);
}

void AccessibilityNodeInfoDataWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  std::string class_name;
  if (GetProperty(AXStringProperty::CLASS_NAME, &class_name)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                 class_name);
  }

  if (!GetProperty(AXBooleanProperty::IMPORTANCE)) {
    out_data->role = ax::mojom::Role::kIgnored;
    return;
  }

  if (GetProperty(AXBooleanProperty::EDITABLE)) {
    out_data->role = ax::mojom::Role::kTextField;
    return;
  }

  if (GetProperty(AXBooleanProperty::HEADING)) {
    out_data->role = ax::mojom::Role::kHeading;
    return;
  }

  if (HasCoveringSpan(AXStringProperty::TEXT, mojom::SpanType::URL) ||
      HasCoveringSpan(AXStringProperty::CONTENT_DESCRIPTION,
                      mojom::SpanType::URL)) {
    out_data->role = ax::mojom::Role::kLink;
    return;
  }

  AXCollectionInfoData* collection_info = node_ptr_->collection_info.get();
  if (collection_info) {
    if (collection_info->row_count > 1 && collection_info->column_count > 1) {
      out_data->role = ax::mojom::Role::kGrid;
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount,
                                collection_info->row_count);
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                                collection_info->column_count);
      return;
    }

    if (collection_info->row_count == 1 || collection_info->column_count == 1) {
      out_data->role = ax::mojom::Role::kList;
      out_data->AddIntAttribute(
          ax::mojom::IntAttribute::kSetSize,
          std::max(collection_info->row_count, collection_info->column_count));
      return;
    }
  }

  AXCollectionItemInfoData* collection_item_info =
      node_ptr_->collection_item_info.get();
  if (collection_item_info) {
    if (collection_item_info->is_heading) {
      out_data->role = ax::mojom::Role::kHeading;
      return;
    }

    // In order to properly resolve the role of this node, a collection item, we
    // need additional information contained only in the CollectionInfo. The
    // CollectionInfo should be an ancestor of this node.
    AXCollectionInfoData* collection_info = nullptr;
    for (AXNodeInfoData* container = node_ptr_; container;) {
      if (!container)
        break;
      if (container->collection_info.get()) {
        collection_info = container->collection_info.get();
        break;
      }

      ArcAccessibilityInfoData* container_data =
          tree_source_->GetParent(tree_source_->GetFromId(container->id));
      if (!container_data->IsNode())
        break;
      container = container_data->GetNode();
    }

    if (collection_info) {
      if (collection_info->row_count > 1 && collection_info->column_count > 1) {
        out_data->role = ax::mojom::Role::kCell;
        out_data->AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex,
                                  collection_item_info->row_index);
        out_data->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnIndex,
                                  collection_item_info->column_index);
        return;
      }

      out_data->role = ax::mojom::Role::kListItem;
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                                std::max(collection_item_info->row_index,
                                         collection_item_info->column_index));
      return;
    }
  }

  std::string chrome_role;
  if (GetProperty(AXStringProperty::CHROME_ROLE, &chrome_role)) {
    ax::mojom::Role role_value = ui::ParseRole(chrome_role.c_str());
    if (role_value != ax::mojom::Role::kNone) {
      // The webView and rootWebArea roles differ between Android and Chrome. In
      // particular, Android includes far fewer attributes which leads to
      // undesirable behavior. Exclude their direct mapping.
      out_data->role = (role_value != ax::mojom::Role::kWebView &&
                        role_value != ax::mojom::Role::kRootWebArea)
                           ? role_value
                           : ax::mojom::Role::kGenericContainer;
      return;
    }
  }

#define MAP_ROLE(android_class_name, chrome_role) \
  if (class_name == android_class_name) {         \
    out_data->role = chrome_role;                 \
    return;                                       \
  }

  // These mappings were taken from accessibility utils (Android -> Chrome) and
  // BrowserAccessibilityAndroid. They do not completely match the above two
  // sources.
  MAP_ROLE(ui::kAXAbsListViewClassname, ax::mojom::Role::kList);
  MAP_ROLE(ui::kAXButtonClassname, ax::mojom::Role::kButton);
  MAP_ROLE(ui::kAXCheckBoxClassname, ax::mojom::Role::kCheckBox);
  MAP_ROLE(ui::kAXCheckedTextViewClassname, ax::mojom::Role::kStaticText);
  MAP_ROLE(ui::kAXCompoundButtonClassname, ax::mojom::Role::kCheckBox);
  MAP_ROLE(ui::kAXDialogClassname, ax::mojom::Role::kDialog);
  MAP_ROLE(ui::kAXEditTextClassname, ax::mojom::Role::kTextField);
  MAP_ROLE(ui::kAXGridViewClassname, ax::mojom::Role::kTable);
  MAP_ROLE(ui::kAXHorizontalScrollViewClassname, ax::mojom::Role::kScrollView);
  MAP_ROLE(ui::kAXImageClassname, ax::mojom::Role::kImage);
  MAP_ROLE(ui::kAXImageButtonClassname, ax::mojom::Role::kButton);
  if (GetProperty(AXBooleanProperty::CLICKABLE)) {
    MAP_ROLE(ui::kAXImageViewClassname, ax::mojom::Role::kButton);
  } else {
    MAP_ROLE(ui::kAXImageViewClassname, ax::mojom::Role::kImage);
  }
  MAP_ROLE(ui::kAXListViewClassname, ax::mojom::Role::kList);
  MAP_ROLE(ui::kAXMenuItemClassname, ax::mojom::Role::kMenuItem);
  MAP_ROLE(ui::kAXPagerClassname, ax::mojom::Role::kGroup);
  MAP_ROLE(ui::kAXProgressBarClassname, ax::mojom::Role::kProgressIndicator);
  MAP_ROLE(ui::kAXRadioButtonClassname, ax::mojom::Role::kRadioButton);
  MAP_ROLE(ui::kAXScrollViewClassname, ax::mojom::Role::kScrollView);
  MAP_ROLE(ui::kAXSeekBarClassname, ax::mojom::Role::kSlider);
  MAP_ROLE(ui::kAXSpinnerClassname, ax::mojom::Role::kPopUpButton);
  MAP_ROLE(ui::kAXSwitchClassname, ax::mojom::Role::kSwitch);
  MAP_ROLE(ui::kAXTabWidgetClassname, ax::mojom::Role::kTabList);
  MAP_ROLE(ui::kAXToggleButtonClassname, ax::mojom::Role::kToggleButton);
  MAP_ROLE(ui::kAXViewClassname, ax::mojom::Role::kGenericContainer);
  MAP_ROLE(ui::kAXViewGroupClassname, ax::mojom::Role::kGroup);

#undef MAP_ROLE

  std::string text;
  GetProperty(AXStringProperty::TEXT, &text);
  if (!text.empty())
    out_data->role = ax::mojom::Role::kStaticText;
  else
    out_data->role = ax::mojom::Role::kGenericContainer;
}

void AccessibilityNodeInfoDataWrapper::PopulateAXState(
    ui::AXNodeData* out_data) const {
#define MAP_STATE(android_boolean_property, chrome_state) \
  if (GetProperty(android_boolean_property))              \
    out_data->AddState(chrome_state);

  // These mappings were taken from accessibility utils (Android -> Chrome) and
  // BrowserAccessibilityAndroid. They do not completely match the above two
  // sources.
  MAP_STATE(AXBooleanProperty::EDITABLE, ax::mojom::State::kEditable);
  MAP_STATE(AXBooleanProperty::FOCUSABLE, ax::mojom::State::kFocusable);
  MAP_STATE(AXBooleanProperty::MULTI_LINE, ax::mojom::State::kMultiline);
  MAP_STATE(AXBooleanProperty::PASSWORD, ax::mojom::State::kProtected);

#undef MAP_STATE

  if (GetProperty(AXBooleanProperty::CHECKABLE)) {
    const bool is_checked = GetProperty(AXBooleanProperty::CHECKED);
    out_data->SetCheckedState(is_checked ? ax::mojom::CheckedState::kTrue
                                         : ax::mojom::CheckedState::kFalse);
  }

  if (!GetProperty(AXBooleanProperty::ENABLED)) {
    out_data->SetRestriction(ax::mojom::Restriction::kDisabled);
  }

  if (!GetProperty(AXBooleanProperty::VISIBLE_TO_USER)) {
    out_data->AddState(ax::mojom::State::kInvisible);
  }
}

void AccessibilityNodeInfoDataWrapper::Serialize(
    ui::AXNodeData* out_data) const {
  // String properties.
  int labelled_by = -1;

  // Accessible name computation picks the first non-empty string from content
  // description, text, labelled by text, or pane title.
  std::string name;
  bool has_name = GetProperty(AXStringProperty::CONTENT_DESCRIPTION, &name);
  if (name.empty())
    has_name |= GetProperty(AXStringProperty::TEXT, &name);
  if (name.empty() && GetProperty(AXIntProperty::LABELED_BY, &labelled_by)) {
    ArcAccessibilityInfoData* labelled_by_node =
        tree_source_->GetFromId(labelled_by);
    if (labelled_by_node && labelled_by_node->IsNode()) {
      ui::AXNodeData labelled_by_data;
      tree_source_->SerializeNode(labelled_by_node, &labelled_by_data);
      has_name |= labelled_by_data.GetStringAttribute(
          ax::mojom::StringAttribute::kName, &name);
    }
  }
  if (name.empty())
    has_name |= GetProperty(AXStringProperty::PANE_TITLE, &name);

  // If it exists, set tooltip value as descritiption on node.
  std::string tooltip;
  if (GetProperty(AXStringProperty::TOOLTIP, &tooltip)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                 tooltip);
    if (GetProperty(AXStringProperty::TEXT, &name)) {
      out_data->SetName(name);
    }
  }

  if (has_name) {
    if (out_data->role == ax::mojom::Role::kTextField)
      out_data->AddStringAttribute(ax::mojom::StringAttribute::kValue, name);
    else
      out_data->SetName(name);
  }

  std::string role_description;
  if (GetProperty(AXStringProperty::ROLE_DESCRIPTION, &role_description)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                 role_description);
  }

  if (out_data->role == ax::mojom::Role::kRootWebArea) {
    std::string package_name;
    if (GetProperty(AXStringProperty::PACKAGE_NAME, &package_name)) {
      const std::string& url =
          base::StringPrintf("%s/%s", package_name.c_str(),
                             tree_source_->tree_id().ToString().c_str());
      out_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
    }
  }

  std::string place_holder;
  if (GetProperty(AXStringProperty::HINT_TEXT, &place_holder)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kPlaceholder,
                                 place_holder);
  }

  // Int properties.
  int traversal_before = -1, traversal_after = -1;
  if (GetProperty(AXIntProperty::TRAVERSAL_BEFORE, &traversal_before)) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kPreviousFocusId,
                              traversal_before);
  }

  if (GetProperty(AXIntProperty::TRAVERSAL_AFTER, &traversal_after)) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kNextFocusId,
                              traversal_after);
  }

  // Boolean properties.
  PopulateAXState(out_data);
  if (GetProperty(AXBooleanProperty::SCROLLABLE)) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable, true);
  }
  if (GetProperty(AXBooleanProperty::CLICKABLE)) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClickable, true);
  }
  if (GetProperty(AXBooleanProperty::SELECTED)) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  }

  // Range info.
  AXRangeInfoData* range_info = node_ptr_->range_info.get();
  if (range_info) {
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                range_info->current);
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                                range_info->min);
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                                range_info->max);
  }

  exo::WMHelper* wm_helper =
      exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance() : nullptr;

  // To get bounds of a node which can be passed to AXNodeData.location,
  // - Root node must exist.
  // - Window where this tree is attached to need to be focused.
  if (tree_source_->GetRoot()->GetId() != -1 && wm_helper) {
    aura::Window* active_window = tree_source_->is_notification()
                                      ? nullptr
                                      : wm_helper->GetActiveWindow();
    const gfx::Rect& local_bounds = tree_source_->GetBounds(
        tree_source_->GetFromId(GetId()), active_window);
    out_data->location.SetRect(local_bounds.x(), local_bounds.y(),
                               local_bounds.width(), local_bounds.height());
  }

  // Integer properties.
  int32_t val;
  if (GetProperty(AXIntProperty::TEXT_SELECTION_START, &val) && val >= 0)
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, val);

  if (GetProperty(AXIntProperty::TEXT_SELECTION_END, &val) && val >= 0)
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, val);

  std::vector<int32_t> standard_action_ids;
  if (GetProperty(AXIntListProperty::STANDARD_ACTION_IDS,
                  &standard_action_ids)) {
    for (size_t i = 0; i < standard_action_ids.size(); ++i) {
      switch (static_cast<AXActionType>(standard_action_ids[i])) {
        case AXActionType::SCROLL_BACKWARD:
          out_data->AddAction(ax::mojom::Action::kScrollBackward);
          break;
        case AXActionType::SCROLL_FORWARD:
          out_data->AddAction(ax::mojom::Action::kScrollForward);
          break;
        default:
          // unmapped
          break;
      }
    }
  }

  // Custom actions.
  std::vector<int32_t> custom_action_ids;
  if (GetProperty(AXIntListProperty::CUSTOM_ACTION_IDS, &custom_action_ids)) {
    std::vector<std::string> custom_action_descriptions;

    CHECK(GetProperty(AXStringListProperty::CUSTOM_ACTION_DESCRIPTIONS,
                      &custom_action_descriptions));
    CHECK(!custom_action_ids.empty());
    CHECK_EQ(custom_action_ids.size(), custom_action_descriptions.size());

    out_data->AddAction(ax::mojom::Action::kCustomAction);
    out_data->AddIntListAttribute(ax::mojom::IntListAttribute::kCustomActionIds,
                                  custom_action_ids);
    out_data->AddStringListAttribute(
        ax::mojom::StringListAttribute::kCustomActionDescriptions,
        custom_action_descriptions);
  }
}

const std::vector<int32_t>* AccessibilityNodeInfoDataWrapper::GetChildren()
    const {
  if (!node_ptr_->int_list_properties)
    return nullptr;
  auto it =
      node_ptr_->int_list_properties->find(AXIntListProperty::CHILD_NODE_IDS);
  if (it == node_ptr_->int_list_properties->end())
    return nullptr;
  return &(it->second);
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXBooleanProperty prop) const {
  if (!node_ptr_->boolean_properties)
    return false;

  auto it = node_ptr_->boolean_properties->find(prop);
  if (it == node_ptr_->boolean_properties->end())
    return false;

  return it->second;
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(AXIntProperty prop,
                                                   int32_t* out_value) const {
  if (!node_ptr_->int_properties)
    return false;

  auto it = node_ptr_->int_properties->find(prop);
  if (it == node_ptr_->int_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool AccessibilityNodeInfoDataWrapper::HasProperty(
    AXStringProperty prop) const {
  if (!node_ptr_->string_properties)
    return false;

  auto it = node_ptr_->string_properties->find(prop);
  return it != node_ptr_->string_properties->end();
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXStringProperty prop,
    std::string* out_value) const {
  if (!HasProperty(prop))
    return false;

  auto it = node_ptr_->string_properties->find(prop);
  *out_value = it->second;
  return true;
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXIntListProperty prop,
    std::vector<int32_t>* out_value) const {
  if (!node_ptr_->int_list_properties)
    return false;

  auto it = node_ptr_->int_list_properties->find(prop);
  if (it == node_ptr_->int_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXStringListProperty prop,
    std::vector<std::string>* out_value) const {
  if (!node_ptr_->string_list_properties)
    return false;

  auto it = node_ptr_->string_list_properties->find(prop);
  if (it == node_ptr_->string_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool AccessibilityNodeInfoDataWrapper::HasCoveringSpan(
    AXStringProperty prop,
    mojom::SpanType span_type) const {
  if (!node_ptr_->spannable_string_properties)
    return false;

  std::string text;
  GetProperty(prop, &text);
  if (text.empty())
    return false;

  auto span_entries_it = node_ptr_->spannable_string_properties->find(prop);
  if (span_entries_it == node_ptr_->spannable_string_properties->end())
    return false;

  for (size_t i = 0; i < span_entries_it->second.size(); ++i) {
    if (span_entries_it->second[i]->span_type != span_type)
      continue;

    size_t span_size =
        span_entries_it->second[i]->end - span_entries_it->second[i]->start;
    if (span_size == text.size())
      return true;
  }
  return false;
}

}  // namespace arc
