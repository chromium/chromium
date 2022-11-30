// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_window_properties.h"

namespace ash {
namespace input_method {

AssistiveWindowProperties::AssistiveWindowProperties() = default;
AssistiveWindowProperties::~AssistiveWindowProperties() = default;

AssistiveWindowProperties::AssistiveWindowProperties(
    const AssistiveWindowProperties& other) = default;
AssistiveWindowProperties& AssistiveWindowProperties::operator=(
    const AssistiveWindowProperties& other) = default;

bool AssistiveWindowProperties::operator==(
    const AssistiveWindowProperties& other) const {
  return type == other.type && visible == other.visible &&
         announce_string == other.announce_string &&
         candidates == other.candidates && show_indices == other.show_indices &&
         show_setting_link == other.show_setting_link;
}

}  // namespace input_method
}  // namespace ash
