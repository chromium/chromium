// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ukm_recorder.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/ui_base_features.h"

namespace keyboard {

void RecordUkmKeyboardShown(ukm::SourceId source,
                            const ui::TextInputType& input_type) {
  if (source == ukm::kInvalidSourceId)
    return;

  ukm::builders::VirtualKeyboard_Open(source)
      .SetTextInputType(input_type)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace keyboard
