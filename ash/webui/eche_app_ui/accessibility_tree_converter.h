// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
#define ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_

#include <optional>

#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_action_data.h"

namespace {

using AXEventData = ax::android::mojom::AccessibilityEventData;
using AXEventType = ax::android::mojom::AccessibilityEventType;
using AXNodeData = ax::android::mojom::AccessibilityNodeInfoData;
using AXWindowData = ax::android::mojom::AccessibilityWindowInfoData;

// Event Properties
using AXEventIntProperty = ax::android::mojom::AccessibilityEventIntProperty;
using AXEventIntListProperty =
    ax::android::mojom::AccessibilityEventIntListProperty;
using AXEventStringProperty =
    ax::android::mojom::AccessibilityEventStringProperty;

// Node Properties
using AXIntProperty = ax::android::mojom::AccessibilityIntProperty;
using AXStringProperty = ax::android::mojom::AccessibilityStringProperty;
using AXIntListProperty = ax::android::mojom::AccessibilityIntListProperty;
using AXBoolProperty = ax::android::mojom::AccessibilityBooleanProperty;

// Window Properties
using AXWindowBoolProperty =
    ax::android::mojom::AccessibilityWindowBooleanProperty;
using AXWindowIntProperty = ax::android::mojom::AccessibilityWindowIntProperty;
using AXWindowIntListProperty =
    ax::android::mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty =
    ax::android::mojom::AccessibilityWindowStringProperty;
using AXWindowType = ax::android::mojom::AccessibilityWindowType;

// Other
using AXRangeType = ax::android::mojom::AccessibilityRangeType;
using AXSelectionMode = ax::android::mojom::AccessibilitySelectionMode;
using AXSpanType = ax::android::mojom::SpanType;
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
  bool DeserializeProto(const std::vector<uint8_t>& serialized_proto,
                        proto::AccessibilityEventData* out_proto);

  mojo::StructPtr<AXEventData> ConvertEventDataProtoToMojom(
      proto::AccessibilityEventData& in_data);

  std::optional<proto::AccessibilityActionData> ConvertActionDataToProto(
      const ui::AXActionData& data,
      int32_t window_id);

 private:
  // Utility Functions
  template <class ProtoType, class MojomType>
  void CopyRepeatedPtrFieldToOptionalVector(
      const ::google::protobuf::RepeatedPtrField<ProtoType>& in_data,
      std::optional<std::vector<MojomType>>& out_data,
      base::RepeatingCallback<MojomType(ProtoType)> transform);

  template <class SharedType>
  void CopyRepeatedPtrFieldToOptionalVector(
      const ::google::protobuf::RepeatedPtrField<SharedType>& in_data,
      std::optional<std::vector<SharedType>>& out_data);

  template <class ProtoPairType, class MojomKeyType, class MojomValueType>
  void ConvertProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPairType>& in_properties,
      std::optional<base::flat_map<MojomKeyType, MojomValueType>>&
          out_properties);

  template <class ProtoPropertyPairType,
            class ProtoValueType,
            class MojomKeyType,
            class MojomValueType>
  bool ConvertListProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
          in_properties,
      std::optional<base::flat_map<MojomKeyType, std::vector<MojomValueType>>>&
          out_properties,
      base::RepeatingCallback<bool(ProtoValueType,
                                   std::optional<MojomValueType>&)> tranform);

  template <class ProtoPropertyPairType,
            class MojomKeyType,
            class SharedValueType>
  bool ConvertListProperties(
      const ::google::protobuf::RepeatedPtrField<ProtoPropertyPairType>&
          in_properties,
      std::optional<base::flat_map<MojomKeyType, std::vector<SharedValueType>>>&
          out_properties);
  // Property Converters
  // Event
  std::optional<AXEventIntProperty> ToMojomProperty(
      const proto::AccessibilityEventIntProperty& property);
  std::optional<AXEventIntListProperty> ToMojomProperty(
      const proto::AccessibilityEventIntListProperty& property);
  std::optional<AXEventStringProperty> ToMojomProperty(
      const proto::AccessibilityEventStringProperty& property);
  // Node
  std::optional<AXIntProperty> ToMojomProperty(
      const proto::AccessibilityIntProperty& property);
  std::optional<AXIntListProperty> ToMojomProperty(
      const proto::AccessibilityIntListProperty& property);
  std::optional<AXStringProperty> ToMojomProperty(
      const proto::AccessibilityStringProperty& property);
  std::optional<AXBoolProperty> ToMojomProperty(
      const proto::AccessibilityBooleanProperty& property);
  // Window
  std::optional<AXWindowIntProperty> ToMojomProperty(
      const proto::AccessibilityWindowIntProperty& property);
  std::optional<AXWindowIntListProperty> ToMojomProperty(
      const proto::AccessibilityWindowIntListProperty& property);
  std::optional<AXWindowStringProperty> ToMojomProperty(
      const proto::AccessibilityWindowStringProperty& property);
  std::optional<AXWindowBoolProperty> ToMojomProperty(
      const proto::AccessibilityWindowBooleanProperty& property);
  // Object converters
  mojo::StructPtr<AXNodeData> ToMojomNodeData(
      const proto::AccessibilityNodeInfoData& node_data);
  mojo::StructPtr<AXWindowData> ToMojomWindowData(
      const proto::AccessibilityWindowInfoData& proto_in);
  // Enum converters
  std::optional<AXEventType> ToMojomEventType(
      const proto::AccessibilityEventType& event_type);
  std::optional<AXRangeType> ToMojomRangeType(
      const proto::AccessibilityRangeType range_type);
  std::optional<AXSpanType> ToMojomSpanType(const proto::SpanType& span_type);
  std::optional<AXSelectionMode> ToMojomSelectionMode(
      const proto::AccessibilitySelectionMode& selection_mode);
  std::optional<AXWindowType> ToMojomWindowType(
      const proto::AccessibilityWindowType& window_type);
};
}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_TREE_CONVERTER_H_
