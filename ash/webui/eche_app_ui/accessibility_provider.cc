
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/accessibility_provider.h"
#include <cstdint>
#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"
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

void AccessibilityProvider::Bind(
    mojo::PendingReceiver<mojom::AccessibilityProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}
}  // namespace ash::eche_app
