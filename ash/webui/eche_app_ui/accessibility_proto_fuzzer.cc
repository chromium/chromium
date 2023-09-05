// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"
#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace ash::eche_app {
DEFINE_PROTO_FUZZER(const proto::AccessibilityEventData& a11y_event_data) {
  size_t nbytes = a11y_event_data.ByteSizeLong();
  std::vector<uint8_t> serialized_proto(nbytes);
  if (nbytes) {
    a11y_event_data.SerializeToArray(serialized_proto.data(), nbytes);
    AccessibilityTreeConverter converter;
    proto::AccessibilityEventData proto_data;
    converter.DeserializeProto(serialized_proto, &proto_data);
    converter.ConvertEventDataProtoToMojom(proto_data);
  }
}
}  // namespace ash::eche_app
