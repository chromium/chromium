// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/base/l10n/l10n_util.h"

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

constexpr mojom::AccessibilityStringProperty
    AccessibilityNodeInfoDataWrapper::text_properties_[];

AccessibilityNodeInfoDataWrapper::AccessibilityNodeInfoDataWrapper(
    AXTreeSourceArc* tree_source,
    AXNodeInfoData* node)
    : AccessibilityInfoDataWrapper(tree_source), node_ptr_(node) {}

AccessibilityNodeInfoDataWrapper::~AccessibilityNodeInfoDataWrapper() = default;

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

bool AccessibilityNodeInfoDataWrapper::IsVirtualNode() const {
  return node_ptr_->is_virtual_node;
}

bool AccessibilityNodeInfoDataWrapper::IsIgnored() const {
  if (!tree_source_->UseFullFocusMode())
    return !IsImportantInAndroid();

  if (!IsImportantInAndroid() || !HasImportantProperty())
    return true;

  if (IsAccessibilityFocusableContainer())
    return false;

  if (!HasText())
    return false;  // A layout container with a11y importance.

  return !HasAccessibilityFocusableText();
}

bool AccessibilityNodeInfoDataWrapper::IsImportantInAndroid() const {
  return IsVirtualNode() || GetProperty(AXBooleanProperty::IMPORTANCE);
}

bool AccessibilityNodeInfoDataWrapper::IsFocusableInFullFocusMode() const {
  if (!IsAccessibilityFocusableContainer() && !HasAccessibilityFocusableText())
    return false;

  ui::AXNodeData data;
  PopulateAXRole(&data);
  return ui::IsControl(data.role) || !ComputeAXName(true).empty();
}

bool AccessibilityNodeInfoDataWrapper::IsAccessibilityFocusableContainer()
    const {
  if (IsVirtualNode()) {
    return GetProperty(AXBooleanProperty::SCREEN_READER_FOCUSABLE) ||
           IsFocusable();
  }

  if (!IsImportantInAndroid() || (IsScrollableContainer() && !HasText()))
    return false;

  return GetProperty(AXBooleanProperty::SCREEN_READER_FOCUSABLE) ||
         IsFocusable() || IsClickable() || IsLongClickable() ||
         IsToplevelScrollItem();
  // TODO(hirokisato): probably check long clickable as well.
}

void AccessibilityNodeInfoDataWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  std::string class_name;
  if (GetProperty(AXStringProperty::CLASS_NAME, &class_name)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                 class_name);
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

  if (node_ptr_->collection_info) {
    AXCollectionInfoData* collection_info = node_ptr_->collection_info.get();
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

  if (node_ptr_->collection_item_info) {
    AXCollectionItemInfoData* collection_item_info =
        node_ptr_->collection_item_info.get();
    if (collection_item_info->is_heading) {
      out_data->role = ax::mojom::Role::kHeading;
      return;
    }

    // In order to properly resolve the role of this node, a collection item, we
    // need additional information contained only in the CollectionInfo. The
    // CollectionInfo should be an ancestor of this node.
    AXCollectionInfoData* collection_info = nullptr;
    for (AccessibilityInfoDataWrapper* container =
             const_cast<AccessibilityNodeInfoDataWrapper*>(this);
         container;) {
      if (!container || !container->IsNode())
        break;
      if (container->IsNode() && container->GetNode()->collection_info) {
        collection_info = container->GetNode()->collection_info.get();
        break;
      }

      container = tree_source_->GetParent(container);
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
    auto role_value = ui::ParseAXEnum<ax::mojom::Role>(chrome_role.c_str());
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
  // EditText is excluded because it can be a container (b/150827734).
  MAP_ROLE(ui::kAXAbsListViewClassname, ax::mojom::Role::kList);
  MAP_ROLE(ui::kAXButtonClassname, ax::mojom::Role::kButton);
  MAP_ROLE(ui::kAXCheckBoxClassname, ax::mojom::Role::kCheckBox);
  MAP_ROLE(ui::kAXCheckedTextViewClassname, ax::mojom::Role::kStaticText);
  MAP_ROLE(ui::kAXCompoundButtonClassname, ax::mojom::Role::kCheckBox);
  MAP_ROLE(ui::kAXDialogClassname, ax::mojom::Role::kDialog);
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
  MAP_ROLE(ui::kAXRadioGroupClassname, ax::mojom::Role::kRadioGroup);
  MAP_ROLE(ui::kAXScrollViewClassname, ax::mojom::Role::kScrollView);
  MAP_ROLE(ui::kAXSeekBarClassname, ax::mojom::Role::kSlider);
  MAP_ROLE(ui::kAXSpinnerClassname, ax::mojom::Role::kPopUpButton);
  MAP_ROLE(ui::kAXSwitchClassname, ax::mojom::Role::kSwitch);
  MAP_ROLE(ui::kAXTabWidgetClassname, ax::mojom::Role::kTabList);
  MAP_ROLE(ui::kAXToggleButtonClassname, ax::mojom::Role::kToggleButton);
  MAP_ROLE(ui::kAXViewClassname, ax::mojom::Role::kGenericContainer);
  MAP_ROLE(ui::kAXViewGroupClassname, ax::mojom::Role::kGroup);

#undef MAP_ROLE

  if (node_ptr_->collection_info) {
    // Fallback for some RecyclerViews which doesn't correctly populate
    // row/col counts.
    out_data->role = ax::mojom::Role::kList;
    return;
  }

  std::string text;
  GetProperty(AXStringProperty::TEXT, &text);
  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(&children);
  if (!text.empty() && children.empty())
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
  MAP_STATE(AXBooleanProperty::MULTI_LINE, ax::mojom::State::kMultiline);
  MAP_STATE(AXBooleanProperty::PASSWORD, ax::mojom::State::kProtected);

#undef MAP_STATE

  const bool focusable = tree_source_->UseFullFocusMode()
                             ? IsAccessibilityFocusableContainer()
                             : IsFocusable();
  if (focusable)
    out_data->AddState(ax::mojom::State::kFocusable);

  if (GetProperty(AXBooleanProperty::CHECKABLE)) {
    const bool is_checked = GetProperty(AXBooleanProperty::CHECKED);
    out_data->SetCheckedState(is_checked ? ax::mojom::CheckedState::kTrue
                                         : ax::mojom::CheckedState::kFalse);
  }

  if (!GetProperty(AXBooleanProperty::ENABLED))
    out_data->SetRestriction(ax::mojom::Restriction::kDisabled);

  if (!GetProperty(AXBooleanProperty::VISIBLE_TO_USER))
    out_data->AddState(ax::mojom::State::kInvisible);

  if (IsIgnored())
    out_data->AddState(ax::mojom::State::kIgnored);
}

void AccessibilityNodeInfoDataWrapper::Serialize(
    ui::AXNodeData* out_data) const {
  AccessibilityInfoDataWrapper::Serialize(out_data);

  bool is_node_tree_root = tree_source_->IsRootOfNodeTree(GetId());
  // String properties that doesn't belong to any of existing chrome
  // automation string properties are pushed into description.
  // TODO(sahok): Refactor this to make clear the functionality(b/158633575).
  std::vector<std::string> descriptions;

  // String properties.
  const std::string name = ComputeAXName(true);
  if (!name.empty())
    out_data->SetName(name);

  // For a textField, the editable text is contained in the text property, and
  // this should be set as the value instead of the name.
  // This ensures that the edited text will be read out appropriately.
  // When the edited text is empty, Android framework shows |hint_text| in
  // the text field and |text| is also populated with |hint_text|.
  // Prevent the duplicated output of |hint_text|.
  if (GetProperty(AXBooleanProperty::EDITABLE) &&
      !GetProperty(AXBooleanProperty::SHOWING_HINT_TEXT)) {
    std::string text;
    GetProperty(AXStringProperty::TEXT, &text);
    if (!text.empty())
      out_data->SetValue(text);
  }

  std::string role_description;
  if (GetProperty(AXStringProperty::ROLE_DESCRIPTION, &role_description)) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                 role_description);
  }

  if (is_node_tree_root) {
    std::string package_name;
    if (GetProperty(AXStringProperty::PACKAGE_NAME, &package_name)) {
      const std::string& url =
          base::StringPrintf("%s/%s", package_name.c_str(),
                             tree_source_->ax_tree_id().ToString().c_str());
      out_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
    }
  }

  // If it exists, set tooltip value as on node.
  std::string tooltip;
  if (GetProperty(AXStringProperty::TOOLTIP, &tooltip))
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kTooltip, tooltip);

  std::string state_description;
  if (GetProperty(AXStringProperty::STATE_DESCRIPTION, &state_description)) {
    // kValue (aria-valuetext) is supported on widgets with range_info. In this
    // case, using kValue over kDescription is closer to the usage of
    // stateDescription.
    if (node_ptr_->range_info) {
      out_data->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                   state_description);
    } else if (GetProperty(AXBooleanProperty::CHECKABLE)) {
      out_data->AddStringAttribute(
          ax::mojom::StringAttribute::kCheckedStateDescription,
          state_description);
    } else {
      descriptions.push_back(state_description);
    }
  }

  // Int properties.
  int traversal_before = -1, traversal_after = -1;
  if (GetProperty(AXIntProperty::TRAVERSAL_BEFORE, &traversal_before)) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kNextFocusId,
                              traversal_before);
  }

  if (GetProperty(AXIntProperty::TRAVERSAL_AFTER, &traversal_after)) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kPreviousFocusId,
                              traversal_after);
  }

  // Boolean properties.
  PopulateAXState(out_data);
  if (GetProperty(AXBooleanProperty::SCROLLABLE))
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable, true);

  if (IsClickable())
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClickable, true);

  if (IsLongClickable()) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kLongClickable, true);
    out_data->AddAction(ax::mojom::Action::kLongClick);
  }

  if (GetProperty(AXBooleanProperty::SELECTED)) {
    if (ui::IsSelectSupported(out_data->role)) {
      out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
    } else {
      descriptions.push_back(
          l10n_util::GetStringUTF8(IDS_ARC_ACCESSIBILITY_SELECTED_STATUS));
    }
  }
  if (GetProperty(AXBooleanProperty::SUPPORTS_TEXT_LOCATION)) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSupportsTextLocation,
                               true);
  }

  // Range info.
  if (node_ptr_->range_info) {
    AXRangeInfoData* range_info = node_ptr_->range_info.get();
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                range_info->current);
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                                range_info->min);
    out_data->AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                                range_info->max);
  }

  // Integer properties.
  int32_t val;
  if (GetProperty(AXIntProperty::TEXT_SELECTION_START, &val) && val >= 0)
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, val);

  if (GetProperty(AXIntProperty::TEXT_SELECTION_END, &val) && val >= 0)
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, val);

  if (GetProperty(AXIntProperty::LIVE_REGION, &val) && val >= 0 &&
      static_cast<mojom::AccessibilityLiveRegionType>(val) !=
          mojom::AccessibilityLiveRegionType::NONE) {
    const std::string& live_status = ToLiveStatusString(
        static_cast<mojom::AccessibilityLiveRegionType>(val));
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                 live_status);
    out_data->AddStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveStatus, live_status);
  }

  // Standard actions.
  if (HasStandardAction(AXActionType::SCROLL_BACKWARD))
    out_data->AddAction(ax::mojom::Action::kScrollBackward);

  if (HasStandardAction(AXActionType::SCROLL_FORWARD))
    out_data->AddAction(ax::mojom::Action::kScrollForward);

  if (HasStandardAction(AXActionType::SCROLL_TO_POSITION))
    out_data->AddAction(ax::mojom::Action::kScrollToPositionAtRowColumn);

  if (HasStandardAction(AXActionType::EXPAND)) {
    out_data->AddAction(ax::mojom::Action::kExpand);
    out_data->AddState(ax::mojom::State::kCollapsed);
  }

  if (HasStandardAction(AXActionType::COLLAPSE)) {
    out_data->AddAction(ax::mojom::Action::kCollapse);
    out_data->AddState(ax::mojom::State::kExpanded);
  }

  if (node_ptr_->standard_actions) {
    for (mojom::AccessibilityActionInAndroidPtr& android_action :
         node_ptr_->standard_actions.value()) {
      if (android_action->label.has_value()) {
        const std::string& label = android_action->label.value();
        const auto action_id =
            static_cast<mojom::AccessibilityActionType>(android_action->id);
        if (action_id == mojom::AccessibilityActionType::CLICK) {
          out_data->AddStringAttribute(
              ax::mojom::StringAttribute::kDoDefaultLabel, label);
        }
        if (action_id == mojom::AccessibilityActionType::LONG_CLICK) {
          out_data->AddStringAttribute(
              ax::mojom::StringAttribute::kLongClickLabel, label);
        }
      }
    }
  }

  // Custom actions.
  if (node_ptr_->custom_actions) {
    std::vector<int32_t> custom_action_ids;
    std::vector<std::string> custom_action_descriptions;

    for (auto& action : node_ptr_->custom_actions.value()) {
      custom_action_ids.push_back(action->id);
      custom_action_descriptions.push_back(action->label.value());
    }

    out_data->AddAction(ax::mojom::Action::kCustomAction);
    out_data->AddIntListAttribute(ax::mojom::IntListAttribute::kCustomActionIds,
                                  custom_action_ids);
    out_data->AddStringListAttribute(
        ax::mojom::StringListAttribute::kCustomActionDescriptions,
        custom_action_descriptions);
  } else if (std::vector<int32_t> custom_action_ids;
             GetProperty(AXIntListProperty::CUSTOM_ACTION_IDS_DEPRECATED,
                         &custom_action_ids)) {
    std::vector<std::string> custom_action_descriptions;

    CHECK(GetProperty(AXStringListProperty::CUSTOM_ACTION_DESCRIPTIONS,
                      &custom_action_descriptions));
    DCHECK(!custom_action_ids.empty());
    DCHECK_EQ(custom_action_ids.size(), custom_action_descriptions.size());

    out_data->AddAction(ax::mojom::Action::kCustomAction);
    out_data->AddIntListAttribute(ax::mojom::IntListAttribute::kCustomActionIds,
                                  custom_action_ids);
    out_data->AddStringListAttribute(
        ax::mojom::StringListAttribute::kCustomActionDescriptions,
        custom_action_descriptions);
  }

  if (!descriptions.empty()) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                 base::JoinString(descriptions, " "));
  }
}

std::string AccessibilityNodeInfoDataWrapper::ComputeAXName(
    bool do_recursive) const {
  // Accessible name computation is a concatenated string comprising of:
  // content description, text, labeled by text, pane title, and cached name
  // from previous events.

  // TODO(sarakato): Exposing all possible labels for a node, may result in
  // too much being spoken. For ARC ++, this may result in divergent behaviour
  // from Talkback.
  std::string text;
  std::string content_description;
  std::string label;
  GetProperty(AXStringProperty::CONTENT_DESCRIPTION, &content_description);
  GetProperty(AXStringProperty::TEXT, &text);

  int labeled_by = -1;
  if (do_recursive && GetProperty(AXIntProperty::LABELED_BY, &labeled_by)) {
    AccessibilityInfoDataWrapper* labeled_by_node =
        tree_source_->GetFromId(labeled_by);
    if (labeled_by_node && labeled_by_node->IsNode())
      label = labeled_by_node->ComputeAXName(false);
  }

  std::string pane_title;
  GetProperty(AXStringProperty::PANE_TITLE, &pane_title);

  // |hint_text| attribute in Android is often used as a placeholder text within
  // textfields.
  std::string hint_text;
  GetProperty(AXStringProperty::HINT_TEXT, &hint_text);

  std::vector<std::string> names;
  // Append non empty properties to name attribute.
  if (!content_description.empty())
    names.push_back(content_description);
  if (!label.empty())
    names.push_back(label);
  if (!pane_title.empty())
    names.push_back(pane_title);
  if (!text.empty() && !GetProperty(AXBooleanProperty::EDITABLE)) {
    // EDITABLE is checked here, as EDITABLE field will have text set as value,
    // this is done in Serialize() function.
    names.push_back(text);
  }
  if (!hint_text.empty())
    names.push_back(hint_text);

  // If a node is accessibility focusable, but has no name, the name should be
  // computed from its descendants.
  if (names.empty() && tree_source_->UseFullFocusMode() &&
      IsAccessibilityFocusableContainer())
    ComputeNameFromContents(&names);

  return base::JoinString(names, " ");
}

void AccessibilityNodeInfoDataWrapper::GetChildren(
    std::vector<AccessibilityInfoDataWrapper*>* children) const {
  if (!node_ptr_->int_list_properties)
    return;
  const auto& it =
      node_ptr_->int_list_properties->find(AXIntListProperty::CHILD_NODE_IDS);
  if (it == node_ptr_->int_list_properties->end())
    return;
  for (const int32_t id : it->second) {
    auto* child = tree_source_->GetFromId(id);
    if (child != nullptr) {
      children->push_back(child);
    } else {
      LOG(WARNING) << "Unexpected nullptr found while GetChildren";
    }
  }
}

int32_t AccessibilityNodeInfoDataWrapper::GetWindowId() const {
  return node_ptr_->window_id;
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXBooleanProperty prop) const {
  return arc::GetBooleanProperty(node_ptr_.get(), prop);
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(AXIntProperty prop,
                                                   int32_t* out_value) const {
  return arc::GetProperty(node_ptr_->int_properties, prop, out_value);
}

bool AccessibilityNodeInfoDataWrapper::HasProperty(
    AXStringProperty prop) const {
  return arc::HasProperty(node_ptr_->string_properties, prop);
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXStringProperty prop,
    std::string* out_value) const {
  return arc::GetProperty(node_ptr_->string_properties, prop, out_value);
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXIntListProperty prop,
    std::vector<int32_t>* out_value) const {
  return arc::GetProperty(node_ptr_->int_list_properties, prop, out_value);
}

bool AccessibilityNodeInfoDataWrapper::GetProperty(
    AXStringListProperty prop,
    std::vector<std::string>* out_value) const {
  return arc::GetProperty(node_ptr_->string_list_properties, prop, out_value);
}

bool AccessibilityNodeInfoDataWrapper::HasStandardAction(
    AXActionType action) const {
  if (node_ptr_->standard_actions) {
    for (const auto& supported_action : node_ptr_->standard_actions.value()) {
      if (static_cast<AXActionType>(supported_action->id) == action)
        return true;
    }
    return false;
  }

  if (!node_ptr_->int_list_properties)
    return false;

  auto itr = node_ptr_->int_list_properties->find(
      AXIntListProperty::STANDARD_ACTION_IDS_DEPRECATED);
  if (itr == node_ptr_->int_list_properties->end())
    return false;

  for (const auto supported_action : itr->second) {
    if (static_cast<AXActionType>(supported_action) == action)
      return true;
  }
  return false;
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

bool AccessibilityNodeInfoDataWrapper::HasText() const {
  if (!IsImportantInAndroid())
    return false;

  for (const auto it : text_properties_) {
    if (HasNonEmptyStringProperty(node_ptr_.get(), it)) {
      return true;
    }
  }
  return false;
}

bool AccessibilityNodeInfoDataWrapper::HasAccessibilityFocusableText() const {
  if (IsVirtualNode())
    return HasText();

  if (!IsImportantInAndroid() || !HasText())
    return false;

  // If any ancestor has a focusable property, the text is used by that node.
  AccessibilityInfoDataWrapper* parent =
      tree_source_->GetFirstImportantAncestor(
          const_cast<AccessibilityNodeInfoDataWrapper*>(this));
  while (parent && parent->IsNode()) {
    if (parent->IsAccessibilityFocusableContainer())
      return false;
    parent = tree_source_->GetFirstImportantAncestor(parent);
  }
  return true;
}

void AccessibilityNodeInfoDataWrapper::ComputeNameFromContents(
    std::vector<std::string>* names) const {
  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(&children);
  for (AccessibilityInfoDataWrapper* child : children) {
    static_cast<AccessibilityNodeInfoDataWrapper*>(child)
        ->ComputeNameFromContentsInternal(names);
  }
}

void AccessibilityNodeInfoDataWrapper::ComputeNameFromContentsInternal(
    std::vector<std::string>* names) const {
  if (IsVirtualNode() || IsAccessibilityFocusableContainer())
    return;

  if (IsImportantInAndroid()) {
    std::string name;
    for (const auto it : text_properties_) {
      if (GetProperty(it, &name) && !name.empty()) {
        // Stop when we get a name for this subtree.
        names->push_back(name);
        return;
      }
    }
  }

  // Otherwise, continue looking for a name in this subtree.
  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(&children);
  for (AccessibilityInfoDataWrapper* child : children) {
    static_cast<AccessibilityNodeInfoDataWrapper*>(child)
        ->ComputeNameFromContentsInternal(names);
  }
}

bool AccessibilityNodeInfoDataWrapper::IsClickable() const {
  return GetProperty(AXBooleanProperty::CLICKABLE) ||
         HasStandardAction(AXActionType::CLICK);
}

bool AccessibilityNodeInfoDataWrapper::IsLongClickable() const {
  return GetProperty(AXBooleanProperty::LONG_CLICKABLE) ||
         HasStandardAction(AXActionType::LONG_CLICK);
}

bool AccessibilityNodeInfoDataWrapper::IsFocusable() const {
  return GetProperty(AXBooleanProperty::FOCUSABLE) ||
         HasStandardAction(AXActionType::FOCUS) ||
         HasStandardAction(AXActionType::CLEAR_FOCUS);
}

bool AccessibilityNodeInfoDataWrapper::IsScrollableContainer() const {
  if (GetProperty(AXBooleanProperty::SCROLLABLE))
    return true;

  ui::AXNodeData data;
  PopulateAXRole(&data);
  return data.role == ax::mojom::Role::kList ||
         data.role == ax::mojom::Role::kGrid ||
         data.role == ax::mojom::Role::kScrollView;
}

bool AccessibilityNodeInfoDataWrapper::IsToplevelScrollItem() const {
  if (!IsVisibleToUser())
    return false;

  AccessibilityInfoDataWrapper* parent =
      tree_source_->GetFirstImportantAncestor(
          const_cast<AccessibilityNodeInfoDataWrapper*>(this));
  if (!parent || !parent->IsNode())
    return false;

  return static_cast<AccessibilityNodeInfoDataWrapper*>(parent)
      ->IsScrollableContainer();
}

bool AccessibilityNodeInfoDataWrapper::HasImportantProperty() const {
  if (!has_important_property_cache_.has_value())
    has_important_property_cache_ = HasImportantPropertyInternal();

  return *has_important_property_cache_;
}

bool AccessibilityNodeInfoDataWrapper::HasImportantPropertyInternal() const {
  if (HasNonEmptyStringProperty(node_ptr_.get(),
                                AXStringProperty::CONTENT_DESCRIPTION) ||
      HasNonEmptyStringProperty(node_ptr_.get(), AXStringProperty::TEXT) ||
      HasNonEmptyStringProperty(node_ptr_.get(),
                                AXStringProperty::PANE_TITLE) ||
      HasNonEmptyStringProperty(node_ptr_.get(), AXStringProperty::HINT_TEXT)) {
    return true;
  }

  if (IsFocusable() || IsClickable() || IsLongClickable())
    return true;

  // These properties are sorted in the same order of mojom file.
  if (GetProperty(AXBooleanProperty::CHECKABLE) ||
      GetProperty(AXBooleanProperty::SELECTED) ||
      GetProperty(AXBooleanProperty::EDITABLE)) {
    return true;
  }

  ui::AXNodeData data;
  PopulateAXRole(&data);
  if (ui::IsControl(data.role))
    return true;

  // Check if any ancestor has an important property.
  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(&children);
  for (AccessibilityInfoDataWrapper* child : children) {
    if (static_cast<AccessibilityNodeInfoDataWrapper*>(child)
            ->HasImportantProperty()) {
      return true;
    }
  }

  return false;
}

}  // namespace arc
