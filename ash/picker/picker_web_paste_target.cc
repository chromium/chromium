// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_web_paste_target.h"

#include <utility>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

PickerWebPasteTarget::PickerWebPasteTarget(GURL url, base::OnceClosure do_paste)
    : url(std::move(url)), do_paste(std::move(do_paste)) {}

PickerWebPasteTarget::PickerWebPasteTarget(PickerWebPasteTarget&&) = default;
PickerWebPasteTarget& PickerWebPasteTarget::operator=(PickerWebPasteTarget&&) =
    default;

PickerWebPasteTarget::~PickerWebPasteTarget() = default;

}  // namespace ash
