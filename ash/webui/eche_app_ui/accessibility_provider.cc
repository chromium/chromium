
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/accessibility_provider.h"

#include <cstdint>
#include "base/notreached.h"

namespace ash::eche_app {

AccessibilityProvider::AccessibilityProvider() = default;
AccessibilityProvider::~AccessibilityProvider() = default;

void AccessibilityProvider::HandleAccessibilityEventReceived(
    const std::vector<uint8_t>& serialized_proto) {
  AccessibilityTreeConverter converter;
  auto mojom_event_data =
      converter.ConvertEventDataProtoToMojom(serialized_proto);
  NOTIMPLEMENTED();
}

void AccessibilityProvider::SetAccessibilityObserver(
    mojo::PendingRemote<mojom::AccessibilityObserver> observer) {
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void AccessibilityProvider::PerformAction(const ui::AXActionData& action) {
  AccessibilityTreeConverter converter;
  auto proto_action = converter.ConvertActionDataToProto(action);
  if (proto_action.has_value()) {
    size_t nbytes = proto_action->ByteSizeLong();
    std::vector<uint8_t> serialized_proto(nbytes);
    proto_action->SerializeToArray(serialized_proto.data(), nbytes);
    observer_remote_->PerformAction(serialized_proto);
  } else {
    LOG(ERROR) << "Failed to serialize AXActionData to protobuf.";
  }
}

void AccessibilityProvider::Bind(
    mojo::PendingReceiver<mojom::AccessibilityProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}
}  // namespace ash::eche_app
