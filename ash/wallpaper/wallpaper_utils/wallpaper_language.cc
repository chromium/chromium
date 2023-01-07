// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"

#include <string>

#include "base/check.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace ash {

// Returns a language tag for the device's locale. If there is a failure while
// obtaining the language tag, an empty string is returned.
std::string GetLanguageTag() {
  auto locale = icu::Locale::getDefault();
  DCHECK(locale.getLanguage());

  UErrorCode status = U_ZERO_ERROR;
  auto language_tag = locale.toLanguageTag<std::string>(status);
  return U_SUCCESS(status) ? language_tag : "";
}

}  // namespace ash
