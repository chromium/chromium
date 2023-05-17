// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
#define ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_

#include <memory>

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "ui/accessibility/ax_action_data.h"

namespace {

using AXEventData = arc::mojom::AccessibilityEventData;
using AXEventType = arc::mojom::AccessibilityEventType;
using AXNodeData = arc::mojom::AccessibilityNodeInfoData;
using AXWindowData = arc::mojom::AccessibilityWindowInfoData;

// Event Properties
using AXEventIntProperty = arc::mojom::AccessibilityEventIntProperty;
using AXEventIntListProperty = arc::mojom::AccessibilityEventIntListProperty;
using AXEventStringProperty = arc::mojom::AccessibilityEventStringProperty;

// Node Properties
using AXIntProperty = arc::mojom::AccessibilityIntProperty;
using AXStringProperty = arc::mojom::AccessibilityStringProperty;
using AXIntListProperty = arc::mojom::AccessibilityIntListProperty;
using AXBoolProperty = arc::mojom::AccessibilityBooleanProperty;

// Window Properties
using AXWindowBoolProperty = arc::mojom::AccessibilityWindowBooleanProperty;
using AXWindowIntProperty = arc::mojom::AccessibilityWindowIntProperty;
using AXWindowIntListProperty = arc::mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty = arc::mojom::AccessibilityWindowStringProperty;
using AXWindowType = arc::mojom::AccessibilityWindowType;

// Other
using AXRangeType = arc::mojom::AccessibilityRangeType;
using AXSelectionMode = arc::mojom::AccessibilitySelectionMode;
using AXSpanType = arc::mojom::SpanType;
}  // namespace

namespace ash::eche_app {

// Converter to convert Android UI tree from proto to mojom format
// https://crsrc.org/c/ash/webui/eche_app_ui/proto/accessibility_mojom.proto
// https://crsrc.org/c/ash/components/arc/mojom/accessibility_helper.mojom
class AccessibilityTreeConverter {
 public:
  AccessibilityTreeConverter();
  ~AccessibilityTreeConverter();
  // Proto is ash/webui/eche_app_ui/proto/accessibility_mojom.proto
  mojo::StructPtr<AXEventData> ConvertEventDataProtoToMojom(
      const std::vector<uint8_t>& serialized_proto);

  absl::optional<proto::AccessibilityActionData> ConvertActionDataToProto(
      const ui::AXActionData& data);

 private:
  // Utility Functions
  template <class ProtoType, class MojomType>
  void CopyRepeatedPtrFieldToOptionalVector(
      const ::google::protobuf::RepeatedPtrField<ProtoType>& in_data,
      absl::optional<std::vector<MojomType>>& out_data,
      base::RepeatingCallback<MojomType(ProtoType)> transform);

  template <class SharedType>
  void CopyRepeatedPtrFieldToOptionalVector(
      const ::google::protobuf::RepeatedPtrField<SharedType>& in_data,
      absl::optional<std::vector<SharedType>>& out_data);

  template <class ProtoPairType, class MojomKeyType, class MojomValueType>
  void ConvertProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPairType>& in_properties,
      absl::optional<base::flat_map<MojomKeyType, MojomValueType>>&
          out_properties);

  template <class ProtoPropertyPairType,
            class ProtoValueType,
            class MojomKeyType,
            class MojomValueType>
  bool ConvertListProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
          in_properties,
      absl::optional<base::flat_map<MojomKeyType, std::vector<MojomValueType>>>&
          out_properties,
      base::RepeatingCallback<bool(ProtoValueType, MojomValueType*)> tranform);

  template <class ProtoPropertyPairType,
            class MojomKeyType,
            class SharedValueType>
  bool ConvertListProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
          in_properties,
      absl::optional<
          base::flat_map<MojomKeyType, std::vector<SharedValueType>>>&
          out_properties);
  // Property Converters
  // Event
  absl::optional<AXEventIntProperty> ToMojomProperty(
      const proto::AccessibilityEventIntProperty& property);
  absl::optional<AXEventIntListProperty> ToMojomProperty(
      const proto::AccessibilityEventIntListProperty& property);
  absl::optional<AXEventStringProperty> ToMojomProperty(
      const proto::AccessibilityEventStringProperty& property);
  // Node
  absl::optional<AXIntProperty> ToMojomProperty(
      const proto::AccessibilityIntProperty& property);
  absl::optional<AXIntListProperty> ToMojomProperty(
      const proto::AccessibilityIntListProperty& property);
  absl::optional<AXStringProperty> ToMojomProperty(
      const proto::AccessibilityStringProperty& property);
  absl::optional<AXBoolProperty> ToMojomProperty(
      const proto::AccessibilityBooleanProperty& property);
  // Window
  absl::optional<AXWindowIntProperty> ToMojomProperty(
      const proto::AccessibilityWindowIntProperty& property);
  absl::optional<AXWindowIntListProperty> ToMojomProperty(
      const proto::AccessibilityWindowIntListProperty& property);
  absl::optional<AXWindowStringProperty> ToMojomProperty(
      const proto::AccessibilityWindowStringProperty& property);
  absl::optional<AXWindowBoolProperty> ToMojomProperty(
      const proto::AccessibilityWindowBooleanProperty& property);
  // Object converters
  mojo::StructPtr<AXNodeData> ToMojomNodeData(
      const proto::AccessibilityNodeInfoData& node_data);
  mojo::StructPtr<AXWindowData> ToMojomWindowData(
      const proto::AccessibilityWindowInfoData& proto_in);
  // Enum converters
  absl::optional<AXEventType> ToMojomEventType(
      const proto::AccessibilityEventType& event_type);
  absl::optional<AXRangeType> ToMojomRangeType(
      const proto::AccessibilityRangeType range_type);
  absl::optional<AXSpanType> ToMojomSpanType(const proto::SpanType& span_type);
  absl::optional<AXSelectionMode> ToMojomSelectionMode(
      const proto::AccessibilitySelectionMode& selection_mode);
  absl::optional<AXWindowType> ToMojomWindowType(
      const proto::AccessibilityWindowType& window_type);
};
}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
