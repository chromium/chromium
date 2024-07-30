// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/picker/picker_web_paste_target.h"

#include <utility>

#include "base/functional/callback.h"

namespace ash {

PickerWebPasteTarget::PickerWebPasteTarget(base::OnceClosure do_paste)
    : do_paste(std::move(do_paste)) {}

PickerWebPasteTarget::PickerWebPasteTarget(PickerWebPasteTarget&&) = default;
PickerWebPasteTarget& PickerWebPasteTarget::operator=(PickerWebPasteTarget&&) =
    default;

PickerWebPasteTarget::~PickerWebPasteTarget() = default;

}  // namespace ash
