// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_rich_media.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace ash {

PickerTextMedia::PickerTextMedia(std::u16string text) : text(std::move(text)) {}

PickerTextMedia::PickerTextMedia(std::string_view text)
    : PickerTextMedia(base::UTF8ToUTF16(text)) {}

PickerImageMedia::PickerImageMedia(GURL url) : url(std::move(url)) {}

PickerLinkMedia::PickerLinkMedia(GURL url) : url(std::move(url)) {}

}  // namespace ash
