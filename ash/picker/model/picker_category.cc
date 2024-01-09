// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_category.h"

#include <string>

namespace ash {

std::u16string GetStringForPickerCategory(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEmojis:
      return u"Emojis";
    case PickerCategory::kSymbols:
      return u"Symbols";
    case PickerCategory::kEmoticons:
      return u"Emoticons";
    case PickerCategory::kGifs:
      return u"Gifs";
  }
}

}  // namespace ash
