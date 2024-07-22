// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/top_row_key_usage_recorder.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_info_metrics.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {

TopRowKeyUsageRecorder::TopRowKeyUsageRecorder() = default;
TopRowKeyUsageRecorder::~TopRowKeyUsageRecorder() = default;

void TopRowKeyUsageRecorder::OnKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->is_repeat() ||
      key_event->type() == ui::EventType::kKeyReleased) {
    return;
  }

  ui::KeyboardCode key_code = key_event->key_code();
  const auto action_key =
      ui::KeyboardCapability::ConvertToTopRowActionKey(key_code);
  if (!action_key) {
    return;
  }

  const auto device_type = Shell::Get()->keyboard_capability()->GetDeviceType(
      key_event->source_device_id());
  if (device_type !=
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"ChromeOS.Inputs.KeyUsage.Internal.",
                    ui::GetTopRowActionKeyName(*action_key)}),
      ui::KeyUsageCategory::kPhysicallyPressed);
}

}  // namespace ash
