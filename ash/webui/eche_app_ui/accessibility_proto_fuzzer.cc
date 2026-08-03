// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"
#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "ash/webui/eche_app_ui/proto/accessibility_mojom_fuzzable.pb.h"
#include "base/check.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace ash::eche_app {

DEFINE_PROTO_FUZZER(
    const fuzzable::ash::eche_app::proto::AccessibilityEventData&
        fuzzable_a11y_event_data) {
  std::string serialized;
  CHECK(fuzzable_a11y_event_data.SerializeToString(&serialized));
  proto::AccessibilityEventData a11y_event_data;
  CHECK(a11y_event_data.ParseFromString(serialized));

  AccessibilityTreeConverter converter;
  converter.ConvertEventDataProtoToMojom(a11y_event_data);
}

}  // namespace ash::eche_app
