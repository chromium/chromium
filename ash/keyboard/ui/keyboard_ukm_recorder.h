// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_UKM_RECORDER_H_
#define ASH_KEYBOARD_UI_KEYBOARD_UKM_RECORDER_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/text_input_type.h"

namespace keyboard {

// Records a keyboard show event in UKM, under the VirtualKeyboard.Open metric.
// Ignores invalid sources.
KEYBOARD_EXPORT void RecordUkmKeyboardShown(
    ukm::SourceId source,
    const ui::TextInputType& input_type);

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_UKM_RECORDER_H_
