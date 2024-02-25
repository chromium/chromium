// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"

namespace ash::eche_app {

AccessibilityTreeConverter::AccessibilityTreeConverter() = default;
AccessibilityTreeConverter::~AccessibilityTreeConverter() = default;

template <class ProtoType, class MojomType>
void AccessibilityTreeConverter::CopyRepeatedPtrFieldToOptionalVector(
    const ::google::protobuf::RepeatedPtrField<ProtoType>& in_data,
    std::optional<std::vector<MojomType>>& out_data,
    base::RepeatingCallback<MojomType(ProtoType)> transform) {
  if (!in_data.empty()) {
    auto out = std::vector<MojomType>();
    for (const auto& item : in_data) {
      MojomType result = transform.Run(item);
      out.emplace_back(std::move(result));
    }
    out_data = std::move(out);
  }
}

template <class SharedType>
void AccessibilityTreeConverter::CopyRepeatedPtrFieldToOptionalVector(
    const ::google::protobuf::RepeatedPtrField<SharedType>& in_data,
    std::optional<std::vector<SharedType>>& out_data) {
  CopyRepeatedPtrFieldToOptionalVector(
      in_data, out_data, base::BindRepeating([](SharedType in) { return in; }));
}

template <class ProtoPairType, class MojomKeyType, class MojomValueType>
void AccessibilityTreeConverter::ConvertProperties(
    const ::google::protobuf::RepeatedPtrField<ProtoPairType>& in_properties,
    std::optional<base::flat_map<MojomKeyType, MojomValueType>>&
        out_properties) {
  if (in_properties.empty()) {
    return;
  }
  out_properties = base::flat_map<MojomKeyType, MojomValueType>();
  for (const ProtoPairType& pair : in_properties) {
    // There is a ToMojomProperty function for various mojom types.
    std::optional<MojomKeyType> key =
        AccessibilityTreeConverter::ToMojomProperty(pair.key());
    if (!key.has_value()) {
      continue;
    }
    (*out_properties)[*key] = pair.value();
  }
}

template <class ProtoPropertyPairType,
          class ProtoValueType,
          class MojomKeyType,
          class MojomValueType>
bool AccessibilityTreeConverter::ConvertListProperties(
    const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
        in_properties,
    std::optional<base::flat_map<MojomKeyType, std::vector<MojomValueType>>>&
        out_properties,
    base::RepeatingCallback<bool(ProtoValueType,
                                 std::optional<MojomValueType>&)> transform) {
  if (in_properties.empty()) {
    return true;
  }
  out_properties = base::flat_map<MojomKeyType, std::vector<MojomValueType>>();
  for (const ProtoPropertyPairType& pair : in_properties) {
    std::optional<MojomKeyType> key =
        AccessibilityTreeConverter::ToMojomProperty(pair.key());
    if (!key.has_value()) {
      continue;
    }
    auto& list = pair.value().values();

    (*out_properties)[*key] = std::vector<MojomValueType>();
    for (const ProtoValueType& value : list) {
      std::optional<MojomValueType> converted;
      bool success = transform.Run(value, converted);
      if (success && converted.has_value()) {
        (*out_properties)[*key].emplace_back(std::move(*converted));
      } else {
        return false;
      }
    }
  }
  return true;
}

template <class ProtoPropertyPairType,
          class MojomKeyType,
          class SharedValueType>
bool AccessibilityTreeConverter::ConvertListProperties(
    const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
        in_properties,
    std::optional<base::flat_map<MojomKeyType, std::vector<SharedValueType>>>&
        out_properties) {
  return ConvertListProperties(
      in_properties, out_properties,
      base::BindRepeating(
          [](SharedValueType in, std::optional<SharedValueType>& out) {
            out = std::move(in);
            return true;
          }));
}

bool AccessibilityTreeConverter::DeserializeProto(
    const std::vector<uint8_t>& serialized_proto,
    proto::AccessibilityEventData* out_proto) {
  CHECK(out_proto);
  return out_proto->ParseFromArray(serialized_proto.data(),
                                   serialized_proto.size());
}

mojo::StructPtr<AXEventData>
AccessibilityTreeConverter::ConvertEventDataProtoToMojom(
    proto::AccessibilityEventData& in_data) {
  auto out_data = AXEventData::New();
  // Details
  auto mojom_event_type = ToMojomEventType(in_data.event_type());
  if (mojom_event_type.has_value()) {
    out_data->event_type = *mojom_event_type;
  }
  out_data->source_id = in_data.source_id();
  // Node info data
  for (const auto& node_data : in_data.node_data()) {
    auto node = ToMojomNodeData(node_data);
    // A node was invalid, invalidate the event data and return.
    if (!node) {
      return nullptr;
    }
    out_data->node_data.emplace_back(std::move(node));
  }
  // Window data
  out_data->window_id = in_data.window_id();
  if (!in_data.window_data().empty()) {
    out_data->window_data = std::vector<mojo::StructPtr<AXWindowData>>();
    for (const auto& window_data : in_data.window_data()) {
      auto converted_window_data = ToMojomWindowData(window_data);
      if (!converted_window_data) {
        return nullptr;
      }
      out_data->window_data->emplace_back(std::move(converted_window_data));
    }
  }
  //  event_text
  CopyRepeatedPtrFieldToOptionalVector(in_data.event_text(),
                                       out_data->event_text);
  // Property lists.
  ConvertProperties(in_data.int_properties(), out_data->int_properties);
  ConvertProperties(in_data.string_properties(), out_data->string_properties);
  if (!ConvertListProperties(in_data.int_list_properties(),
                             out_data->int_list_properties)) {
    // Some properties were invalid.
    return nullptr;
  }
  return out_data;
}

std::optional<proto::AccessibilityActionType> ConvertType(
    ax::mojom::Action action_type) {
  switch (action_type) {
    case ax::mojom::Action::kDoDefault:
      return proto::AccessibilityActionType::ACTION_CLICK;
    case ax::mojom::Action::kFocus:
      return proto::AccessibilityActionType::ACTION_ACCESSIBILITY_ACTION_FOCUS;
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      return proto::AccessibilityActionType::ACTION_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kScrollToMakeVisible:
      return proto::AccessibilityActionType::ACTION_SHOW_ON_SCREEN;
    case ax::mojom::Action::kScrollBackward:
      return proto::AccessibilityActionType::ACTION_SCROLL_BACKWARD;
    case ax::mojom::Action::kScrollForward:
      return proto::AccessibilityActionType::ACTION_SCROLL_FORWARD;
    case ax::mojom::Action::kScrollUp:
      return proto::AccessibilityActionType::ACTION_SCROLL_UP;
    case ax::mojom::Action::kScrollDown:
      return proto::AccessibilityActionType::ACTION_SCROLL_DOWN;
    case ax::mojom::Action::kScrollLeft:
      return proto::AccessibilityActionType::ACTION_SCROLL_LEFT;
    case ax::mojom::Action::kScrollRight:
      return proto::AccessibilityActionType::ACTION_SCROLL_RIGHT;
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
      return proto::AccessibilityActionType::ACTION_SCROLL_TO_POSITION;
    case ax::mojom::Action::kCustomAction:
      return proto::AccessibilityActionType::ACTION_CUSTOM_ACTION;
    case ax::mojom::Action::kSetAccessibilityFocus:
      return proto::AccessibilityActionType::ACTION_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kClearAccessibilityFocus:
      return proto::AccessibilityActionType::ACTION_CLEAR_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kGetTextLocation:
      return proto::AccessibilityActionType::ACTION_GET_TEXT_LOCATION;
    case ax::mojom::Action::kShowTooltip:
      return proto::AccessibilityActionType::ACTION_SHOW_TOOLTIP;
    case ax::mojom::Action::kHideTooltip:
      return proto::AccessibilityActionType::ACTION_HIDE_TOOLTIP;
    case ax::mojom::Action::kCollapse:
      return proto::AccessibilityActionType::ACTION_COLLAPSE;
    case ax::mojom::Action::kExpand:
      return proto::AccessibilityActionType::ACTION_EXPAND;
    case ax::mojom::Action::kLongClick:
      return proto::AccessibilityActionType::ACTION_LONG_CLICK;
    default:
      return std::nullopt;
  }
}

void PopulateActionParameters(const ui::AXActionData& chrome_data,
                              proto::AccessibilityActionData& action_data) {
  switch (action_data.action_type()) {
    case proto::AccessibilityActionType::ACTION_SCROLL_TO_POSITION: {
      const auto [row, column] = chrome_data.row_column;
      auto* row_kvp = action_data.add_int_parameters();
      row_kvp->set_key(proto::ActionIntArgumentType::TYPE_ROW_INT);
      row_kvp->set_value(row);
      auto* col_kvp = action_data.add_int_parameters();
      col_kvp->set_key(proto::ActionIntArgumentType::TYPE_COLUMN_INT);
      col_kvp->set_value(column);
      break;
    }
    case proto::AccessibilityActionType::ACTION_CUSTOM_ACTION:
      action_data.set_custom_action_id(chrome_data.custom_action_id);
      break;
    case proto::AccessibilityActionType::ACTION_NEXT_HTML_ELEMENT:
    case proto::AccessibilityActionType::ACTION_PREVIOUS_HTML_ELEMENT:
    case proto::AccessibilityActionType::ACTION_ACCESSIBILITY_ACTION_FOCUS:
    case proto::AccessibilityActionType::ACTION_CLEAR_FOCUS:
    case proto::AccessibilityActionType::ACTION_SELECT:
    case proto::AccessibilityActionType::ACTION_CLEAR_SELECTION:
    case proto::AccessibilityActionType::ACTION_CLICK:
    case proto::AccessibilityActionType::ACTION_LONG_CLICK:
    case proto::AccessibilityActionType::ACTION_ACCESSIBILITY_FOCUS:
    case proto::AccessibilityActionType::ACTION_CLEAR_ACCESSIBILITY_FOCUS:
    case proto::AccessibilityActionType::ACTION_NEXT_AT_MOVEMENT_GRANULARITY:
    case proto::AccessibilityActionType::
        ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY:
    case proto::AccessibilityActionType::ACTION_SCROLL_FORWARD:
    case proto::AccessibilityActionType::ACTION_SCROLL_BACKWARD:
    case proto::AccessibilityActionType::ACTION_COPY:
    case proto::AccessibilityActionType::ACTION_PASTE:
    case proto::AccessibilityActionType::ACTION_CUT:
    case proto::AccessibilityActionType::ACTION_SET_SELECTION:
    case proto::AccessibilityActionType::ACTION_EXPAND:
    case proto::AccessibilityActionType::ACTION_COLLAPSE:
    case proto::AccessibilityActionType::ACTION_DISMISS:
    case proto::AccessibilityActionType::ACTION_SET_TEXT:
    case proto::AccessibilityActionType::ACTION_CONTEXT_CLICK:
    case proto::AccessibilityActionType::ACTION_SCROLL_DOWN:
    case proto::AccessibilityActionType::ACTION_SCROLL_LEFT:
    case proto::AccessibilityActionType::ACTION_SCROLL_RIGHT:
    case proto::AccessibilityActionType::ACTION_SCROLL_UP:
    case proto::AccessibilityActionType::ACTION_SET_PROGRESS:
    case proto::AccessibilityActionType::ACTION_SHOW_ON_SCREEN:
    case proto::AccessibilityActionType::ACTION_GET_TEXT_LOCATION:
    case proto::AccessibilityActionType::ACTION_SHOW_TOOLTIP:
    case proto::AccessibilityActionType::ACTION_HIDE_TOOLTIP:
    case proto::AccessibilityActionType::
        AccessibilityActionType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityActionType::
        AccessibilityActionType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
}

std::optional<proto::AccessibilityActionData>
AccessibilityTreeConverter::ConvertActionDataToProto(
    const ui::AXActionData& data,
    int32_t window_id) {
  proto::AccessibilityActionData action_data;

  auto action_type = ConvertType(data.action);
  if (!action_type.has_value()) {
    return std::nullopt;
  }

  action_data.set_window_id(window_id);

  action_data.set_action_type(action_type.value());
  action_data.set_node_id(data.target_node_id);
  PopulateActionParameters(data, action_data);

  if (action_type == proto::AccessibilityActionType::ACTION_GET_TEXT_LOCATION) {
    action_data.set_start_index(data.start_index);
    action_data.set_end_index(data.end_index);
  }
  return action_data;
}

// Object converters
mojo::StructPtr<AXWindowData> AccessibilityTreeConverter::ToMojomWindowData(
    const proto::AccessibilityWindowInfoData& proto_in) {
  // Create object.
  auto mojom_out = AXWindowData::New();

  // id
  mojom_out->window_id = proto_in.window_id();
  // Root node id
  mojom_out->root_node_id = proto_in.root_node_id();
  // Bounds
  if (proto_in.has_bounds_in_screen()) {
    const auto& proto_bounds = proto_in.bounds_in_screen();
    if (proto_bounds.bottom() > proto_bounds.top() &&
        proto_bounds.right() > proto_bounds.left()) {
      mojom_out->bounds_in_screen.SetByBounds(
          proto_bounds.left(), proto_bounds.top(), proto_bounds.right(),
          proto_bounds.bottom());
    } else {
      // Window had invalid data.
      return nullptr;
    }
  }

  // Window type
  auto mojom_window_type = ToMojomWindowType(proto_in.window_type());
  if (mojom_window_type.has_value()) {
    mojom_out->window_type = *mojom_window_type;
  }

  // Properties
  ConvertProperties(proto_in.boolean_properties(),
                    mojom_out->boolean_properties);
  ConvertProperties(proto_in.int_properties(), mojom_out->int_properties);
  ConvertProperties(proto_in.string_properties(), mojom_out->string_properties);
  ConvertListProperties(proto_in.int_list_properties(),
                        mojom_out->int_list_properties);
  return mojom_out;
}

mojo::StructPtr<AXNodeData> AccessibilityTreeConverter::ToMojomNodeData(
    const proto::AccessibilityNodeInfoData& proto_in) {
  // Create object.
  auto mojom_out = AXNodeData::New();
  // Bounds
  if (proto_in.has_bounds_in_screen()) {
    const auto& proto_bounds = proto_in.bounds_in_screen();
    if (proto_bounds.bottom() < proto_bounds.top() ||
        proto_bounds.right() < proto_bounds.left()) {
      return nullptr;
    }
    mojom_out->bounds_in_screen.SetByBounds(
        proto_bounds.left(), proto_bounds.top(), proto_bounds.right(),
        proto_bounds.bottom());
  }
  // Id
  mojom_out->id = proto_in.id();
  // Properties
  ConvertProperties(proto_in.boolean_properties(),
                    mojom_out->boolean_properties);
  ConvertProperties(proto_in.int_properties(), mojom_out->int_properties);
  ConvertProperties(proto_in.string_properties(), mojom_out->string_properties);
  ConvertListProperties(proto_in.int_list_properties(),
                        mojom_out->int_list_properties);
  bool convert_success = ConvertListProperties(
      proto_in.spannable_string_properties(),
      mojom_out->spannable_string_properties,
      base::BindRepeating(
          [](AccessibilityTreeConverter* converter, proto::SpanEntry entry,
             std::optional<ax::android::mojom::SpanEntryPtr>& out_entry) {
            auto result_ptr = ax::android::mojom::SpanEntry::New();
            if (entry.start() >= entry.end()) {
              return false;
            }
            result_ptr->end = entry.end();
            result_ptr->start = entry.start();
            auto mojom_span_type =
                converter->ToMojomSpanType(entry.span_type());
            if (mojom_span_type.has_value()) {
              result_ptr->span_type = *mojom_span_type;
            }
            out_entry = std::move(result_ptr);
            return true;
          },
          this));

  // Some properties were invalid.
  if (!convert_success) {
    return nullptr;
  }

  // Collection Info
  if (proto_in.has_collection_info()) {
    const auto& proto_collection_info = proto_in.collection_info();
    mojom_out->collection_info =
        ax::android::mojom::AccessibilityCollectionInfoData::New();
    mojom_out->collection_info->column_count =
        proto_collection_info.column_count();
    mojom_out->collection_info->row_count = proto_collection_info.row_count();
    mojom_out->collection_info->is_hierarchical =
        proto_collection_info.is_hierarchical();
    auto collection_selection_mode =
        ToMojomSelectionMode(proto_collection_info.selection_mode());
    if (collection_selection_mode.has_value()) {
      mojom_out->collection_info->selection_mode = *collection_selection_mode;
    }
  }
  // Collection Item Info
  if (proto_in.has_collection_item_info()) {
    const auto& proto_collection_item_info = proto_in.collection_item_info();
    mojom_out->collection_item_info =
        ax::android::mojom::AccessibilityCollectionItemInfoData::New();
    mojom_out->collection_item_info->row_index =
        proto_collection_item_info.row_index();
    mojom_out->collection_item_info->column_index =
        proto_collection_item_info.column_index();
    mojom_out->collection_item_info->row_span =
        proto_collection_item_info.row_span();
    mojom_out->collection_item_info->column_span =
        proto_collection_item_info.column_span();
    mojom_out->collection_item_info->is_heading =
        proto_collection_item_info.is_heading();
    mojom_out->collection_item_info->is_selected =
        proto_collection_item_info.is_selected();
  }
  // Range info
  if (proto_in.has_range_info()) {
    if (proto_in.range_info().min() <= proto_in.range_info().max()) {
      mojom_out->range_info->min = proto_in.range_info().min();
      mojom_out->range_info->max = proto_in.range_info().max();
    }
    mojom_out->range_info->current = proto_in.range_info().current();
    auto mojom_range_type =
        ToMojomRangeType(proto_in.range_info().range_type());
    if (mojom_range_type.has_value()) {
      mojom_out->range_info->range_type = *mojom_range_type;
    }
  }

  // Window Id
  mojom_out->window_id = proto_in.window_id();

  // Virtual Node
  mojom_out->is_virtual_node = proto_in.is_virtual_node();

  // Standard Actions
  CopyRepeatedPtrFieldToOptionalVector(
      proto_in.standard_actions(), mojom_out->standard_actions,
      base::BindRepeating([](proto::AccessibilityActionInHost proto_action) {
        auto result_ptr =
            ax::android::mojom::AccessibilityActionInAndroid::New();
        result_ptr->id = proto_action.id();
        result_ptr->label = proto_action.label();
        return result_ptr;
      }));
  // Custom Actions
  CopyRepeatedPtrFieldToOptionalVector(
      proto_in.custom_actions(), mojom_out->custom_actions,
      base::BindRepeating([](proto::AccessibilityActionInHost proto_action) {
        auto result_ptr =
            ax::android::mojom::AccessibilityActionInAndroid::New();
        result_ptr->id = proto_action.id();
        result_ptr->label = proto_action.label();
        return result_ptr;
      }));
  return mojom_out;
}

// Enum converters
std::optional<AXEventType> AccessibilityTreeConverter::ToMojomEventType(
    const proto::AccessibilityEventType& event_type) {
  switch (event_type) {
    case proto::TYPE_VIEW_FOCUSED:
      return AXEventType::VIEW_FOCUSED;
    case proto::TYPE_VIEW_CLICKED:
      return AXEventType::VIEW_CLICKED;
    case proto::TYPE_VIEW_LONG_CLICKED:
      return AXEventType::VIEW_LONG_CLICKED;
    case proto::TYPE_VIEW_SELECTED:
      return AXEventType::VIEW_SELECTED;
    case proto::TYPE_VIEW_TEXT_CHANGED:
      return AXEventType::VIEW_TEXT_CHANGED;
    case proto::TYPE_WINDOW_STATE_CHANGED:
      return AXEventType::WINDOW_STATE_CHANGED;
    case proto::TYPE_VIEW_HOVER_ENTER:
      return AXEventType::VIEW_HOVER_ENTER;
    case proto::TYPE_VIEW_HOVER_EXIT:
      return AXEventType::VIEW_HOVER_EXIT;
    case proto::TYPE_TOUCH_EXPLORATION_GESTURE_START:
      return AXEventType::TOUCH_EXPLORATION_GESTURE_START;
    case proto::TYPE_TOUCH_EXPLORATION_GESTURE_END:
      return AXEventType::TOUCH_EXPLORATION_GESTURE_END;
    case proto::TYPE_WINDOW_CONTENT_CHANGED:
      return AXEventType::WINDOW_CONTENT_CHANGED;
    case proto::TYPE_VIEW_SCROLLED:
      return AXEventType::VIEW_SCROLLED;
    case proto::TYPE_VIEW_TEXT_SELECTION_CHANGED:
      return AXEventType::VIEW_TEXT_SELECTION_CHANGED;
    case proto::TYPE_ANNOUNCEMENT:
      return AXEventType::ANNOUNCEMENT;
    case proto::TYPE_VIEW_ACCESSIBILITY_FOCUSED:
      return AXEventType::VIEW_ACCESSIBILITY_FOCUSED;
    case proto::TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED:
      return AXEventType::VIEW_ACCESSIBILITY_FOCUS_CLEARED;
    case proto::TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
      return AXEventType::VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY;
    case proto::TYPE_GESTURE_DETECTION_START:
      return AXEventType::GESTURE_DETECTION_START;
    case proto::TYPE_GESTURE_DETECTION_END:
      return AXEventType::GESTURE_DETECTION_END;
    case proto::TYPE_TOUCH_INTERACTION_START:
      return AXEventType::TOUCH_INTERACTION_START;
    case proto::TYPE_TOUCH_INTERACTION_END:
      return AXEventType::TOUCH_INTERACTION_END;
    case proto::TYPE_WINDOWS_CHANGED:
      return AXEventType::WINDOWS_CHANGED;
    case proto::TYPE_VIEW_CONTEXT_CLICKED:
      return AXEventType::VIEW_CONTEXT_CLICKED;
    case proto::TYPE_ASSIST_READING_CONTEXT:
      return AXEventType::ASSIST_READING_CONTEXT;
    case proto::TYPE_EVENT_TYPE_UNKNOWN:
    case proto::AccessibilityEventType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityEventType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

std::optional<AXRangeType> AccessibilityTreeConverter::ToMojomRangeType(
    const proto::AccessibilityRangeType range_type) {
  switch (range_type) {
    case proto::TYPE_INT:
      return AXRangeType::INT;
    case proto::TYPE_FLOAT:
      return AXRangeType::FLOAT;
    case proto::TYPE_PERCENT:
      return AXRangeType::PERCENT;
    case proto::AccessibilityRangeType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityRangeType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

std::optional<AXSelectionMode> AccessibilityTreeConverter::ToMojomSelectionMode(
    const proto::AccessibilitySelectionMode& selection_mode) {
  switch (selection_mode) {
    case proto::MODE_ACCESSIBILITY_SELECTION_MODE_NONE:
      return AXSelectionMode::NONE;
    case proto::MODE_SINGLE:
      return AXSelectionMode::SINGLE;
    case proto::MODE_MULTIPLE:
      return AXSelectionMode::MULTIPLE;
    case proto::AccessibilitySelectionMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilitySelectionMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXWindowType> AccessibilityTreeConverter::ToMojomWindowType(
    const proto::AccessibilityWindowType& window_type) {
  switch (window_type) {
    case proto::TYPE_ACCESSIBILITY_OVERLAY:
      return AXWindowType::TYPE_ACCESSIBILITY_OVERLAY;
    case proto::TYPE_APPLICATION:
      return AXWindowType::TYPE_APPLICATION;
    case proto::TYPE_SPLIT_SCREEN_DIVIDER:
      return AXWindowType::TYPE_SPLIT_SCREEN_DIVIDER;
    case proto::TYPE_SYSTEM:
      return AXWindowType::TYPE_SYSTEM;
    case proto::TYPE_WINDOW_TYPE_UNKNOWN:
    case proto::AccessibilityWindowType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityWindowType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

std::optional<AXSpanType> AccessibilityTreeConverter::ToMojomSpanType(
    const proto::SpanType& span_type) {
  switch (span_type) {
    case proto::TYPE_URL:
      return AXSpanType::URL;
    case proto::TYPE_SPAN_TYPE_CLICKABLE:
      return AXSpanType::CLICKABLE;
    case proto::SpanType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::SpanType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

// Property Converters
std::optional<AXEventIntProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityEventIntProperty& property) {
  switch (property) {
    case proto::PROPERTY_ACTION:
      return AXEventIntProperty::ACTION;
    case proto::PROPERTY_FROM_INDEX:
      return AXEventIntProperty::FROM_INDEX;
    case proto::PROPERTY_TO_INDEX:
      return AXEventIntProperty::TO_INDEX;
    case proto::PROPERTY_ITEM_COUNT:
      return AXEventIntProperty::ITEM_COUNT;
    case proto::PROPERTY_CURRENT_ITEM_INDEX:
      return AXEventIntProperty::CURRENT_ITEM_INDEX;
    case proto::PROPERTY_SCROLL_X:
      return AXEventIntProperty::SCROLL_X;
    case proto::PROPERTY_SCROLL_Y:
      return AXEventIntProperty::SCROLL_Y;
    case proto::PROPERTY_MAX_SCROLL_X:
      return AXEventIntProperty::MAX_SCROLL_X;
    case proto::PROPERTY_MAX_SCROLL_Y:
      return AXEventIntProperty::MAX_SCROLL_Y;
    case proto::PROPERTY_SCROLL_DELTA_X:
      return AXEventIntProperty::SCROLL_DELTA_X;
    case proto::PROPERTY_SCROLL_DELTA_Y:
      return AXEventIntProperty::SCROLL_DELTA_Y;
    case proto::AccessibilityEventIntProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityEventIntProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXEventIntListProperty>
AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityEventIntListProperty& property) {
  switch (property) {
    case proto::PROPERTY_CONTENT_CHANGE_TYPES:
      return AXEventIntListProperty::CONTENT_CHANGE_TYPES;
    case proto::AccessibilityEventIntListProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityEventIntListProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXEventStringProperty>
AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityEventStringProperty& property) {
  switch (property) {
    case proto::PROPERTY_ACCESSIBILITY_EVENT_CLASS_NAME:
      return AXEventStringProperty::CLASS_NAME;
    case proto::PROPERTY_ACCESSIBILITY_EVENT_PACKAGE_NAME:
      return AXEventStringProperty::PACKAGE_NAME;
    case proto::PROPERTY_ACCESSIBILITY_EVENT_CONTENT_DESCRIPTION:
      return AXEventStringProperty::CONTENT_DESCRIPTION;
    case proto::AccessibilityEventStringProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityEventStringProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXIntProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityIntProperty& property) {
  switch (property) {
    case proto::PROPERTY_LABEL_FOR:
      return AXIntProperty::LABEL_FOR;
    case proto::PROPERTY_LABELED_BY:
      return AXIntProperty::LABELED_BY;
    case proto::PROPERTY_TRAVERSAL_BEFORE:
      return AXIntProperty::TRAVERSAL_BEFORE;
    case proto::PROPERTY_TRAVERSAL_AFTER:
      return AXIntProperty::TRAVERSAL_AFTER;
    case proto::PROPERTY_MAX_TEXT_LENGTH:
      return AXIntProperty::MAX_TEXT_LENGTH;
    case proto::PROPERTY_TEXT_SELECTION_START:
      return AXIntProperty::TEXT_SELECTION_START;
    case proto::PROPERTY_TEXT_SELECTION_END:
      return AXIntProperty::TEXT_SELECTION_END;
    case proto::PROPERTY_INPUT_TYPE:
      return AXIntProperty::INPUT_TYPE;
    case proto::PROPERTY_LIVE_REGION:
      return AXIntProperty::LIVE_REGION;
    case proto::AccessibilityIntProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityIntProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXIntListProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityIntListProperty& property) {
  switch (property) {
    case proto::PROPERTY_CHILD_NODE_IDS:
      return AXIntListProperty::CHILD_NODE_IDS;
    case proto::AccessibilityIntListProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityIntListProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXStringProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityStringProperty& property) {
  switch (property) {
    case proto::PROPERTY_ACCESSIBILITY_PACKAGE_NAME:
      return AXStringProperty::PACKAGE_NAME;
    case proto::PROPERTY_ACCESSIBILITY_CLASS_NAME:
      return AXStringProperty::CLASS_NAME;
    case proto::PROPERTY_ACCESSIBILITY_TEXT:
      return AXStringProperty::TEXT;
    case proto::PROPERTY_ACCESSIBILITY_CONTENT_DESCRIPTION:
      return AXStringProperty::CONTENT_DESCRIPTION;
    case proto::PROPERTY_VIEW_ID_RESOURCE_NAME:
      return AXStringProperty::VIEW_ID_RESOURCE_NAME;
    case proto::PROPERTY_CHROME_ROLE:
      return AXStringProperty::CHROME_ROLE;
    case proto::PROPERTY_ROLE_DESCRIPTION:
      return AXStringProperty::ROLE_DESCRIPTION;
    case proto::PROPERTY_TOOLTIP:
      return AXStringProperty::TOOLTIP;
    case proto::PROPERTY_ACCESSIBILITY_PANE_TITLE:
      return AXStringProperty::PANE_TITLE;
    case proto::PROPERTY_HINT_TEXT:
      return AXStringProperty::HINT_TEXT;
    case proto::PROPERTY_ACCESSIBILITY_STATE_DESCRIPTION:
      return AXStringProperty::STATE_DESCRIPTION;
    case proto::AccessibilityStringProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityStringProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXBoolProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityBooleanProperty& property) {
  switch (property) {
    case proto::PROPERTY_CHECKABLE:
      return AXBoolProperty::CHECKABLE;
    case proto::PROPERTY_CHECKED:
      return AXBoolProperty::CHECKED;
    case proto::PROPERTY_FOCUSABLE:
      return AXBoolProperty::FOCUSABLE;
    case proto::PROPERTY_ACCESSIBILITY_BOOLEAN_PROPERTY_FOCUSED:
      return AXBoolProperty::ACCESSIBILITY_FOCUSED;
    case proto::PROPERTY_SELECTED:
      return AXBoolProperty::SELECTED;
    case proto::PROPERTY_ACCESSIBILITY_BOOLEAN_CLICKABLE:
      return AXBoolProperty::CLICKABLE;
    case proto::PROPERTY_LONG_CLICKABLE:
      return AXBoolProperty::LONG_CLICKABLE;
    case proto::PROPERTY_ENABLED:
      return AXBoolProperty::ENABLED;
    case proto::PROPERTY_PASSWORD:
      return AXBoolProperty::PASSWORD;
    case proto::PROPERTY_SCROLLABLE:
      return AXBoolProperty::SCROLLABLE;
    case proto::PROPERTY_ACCESSIBILITY_BOOLEAN_FOCUSED:
      return AXBoolProperty::FOCUSED;
    case proto::PROPERTY_VISIBLE_TO_USER:
      return AXBoolProperty::VISIBLE_TO_USER;
    case proto::PROPERTY_EDITABLE:
      return AXBoolProperty::EDITABLE;
    case proto::PROPERTY_OPENS_POPUP:
      return AXBoolProperty::OPENS_POPUP;
    case proto::PROPERTY_DISMISSABLE:
      return AXBoolProperty::DISMISSABLE;
    case proto::PROPERTY_MULTI_LINE:
      return AXBoolProperty::MULTI_LINE;
    case proto::PROPERTY_CONTENT_INVALID:
      return AXBoolProperty::CONTENT_INVALID;
    case proto::PROPERTY_CONTEXT_CLICKABLE:
      return AXBoolProperty::CONTEXT_CLICKABLE;
    case proto::PROPERTY_IMPORTANCE:
      return AXBoolProperty::IMPORTANCE;
    case proto::PROPERTY_SCREEN_READER_FOCUSABLE:
      return AXBoolProperty::SCREEN_READER_FOCUSABLE;
    case proto::PROPERTY_SHOWING_HINT_TEXT:
      return AXBoolProperty::SHOWING_HINT_TEXT;
    case proto::PROPERTY_HEADING:
      return AXBoolProperty::HEADING;
    case proto::PROPERTY_SUPPORTS_TEXT_LOCATION:
      return AXBoolProperty::SUPPORTS_TEXT_LOCATION;
    case proto::AccessibilityBooleanProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityBooleanProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

std::optional<AXWindowIntProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityWindowIntProperty& property) {
  switch (property) {
    case proto::PROPERTY_ANCHOR_NODE_ID:
      return AXWindowIntProperty::ANCHOR_NODE_ID;
    case proto::PROPERTY_LAYER_ORDER:
      return AXWindowIntProperty::LAYER_ORDER;
    case proto::PROPERTY_PARENT_WINDOW_ID:
      return AXWindowIntProperty::PARENT_WINDOW_ID;
    case proto::AccessibilityWindowIntProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityWindowIntProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXWindowIntListProperty>
AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityWindowIntListProperty& property) {
  switch (property) {
    case proto::PROPERTY_CHILD_WINDOW_IDS:
      return AXWindowIntListProperty::CHILD_WINDOW_IDS;
    case proto::AccessibilityWindowIntListProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityWindowIntListProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXWindowStringProperty>
AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityWindowStringProperty& property) {
  switch (property) {
    case proto::PROPERTY_TITLE:
      return AXWindowStringProperty::TITLE;
    case proto::AccessibilityWindowStringProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityWindowStringProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}
std::optional<AXWindowBoolProperty> AccessibilityTreeConverter::ToMojomProperty(
    const proto::AccessibilityWindowBooleanProperty& property) {
  switch (property) {
    case proto::PROPERTY_ACCESSIBILITY_FOCUSED:
      return AXWindowBoolProperty::ACCESSIBILITY_FOCUSED;
    case proto::PROPERTY_FOCUSED:
      return AXWindowBoolProperty::FOCUSED;
    case proto::PROPERTY_IN_PICTURE_IN_PICTURE_MODE:
      return AXWindowBoolProperty::IN_PICTURE_IN_PICTURE_MODE;
    case proto::PROPERTY_WINDOW_ACTIVE:
      return AXWindowBoolProperty::WINDOW_ACTIVE;
    case proto::AccessibilityWindowBooleanProperty_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::AccessibilityWindowBooleanProperty_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  return std::nullopt;
}

}  // namespace ash::eche_app
