// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"

#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"

namespace ash {

// Returns a language tag for the device's locale.
base::i18n::LanguageTag GetLanguageTag() {
  return base::i18n::GetDefaultIcuLocale();
}

}  // namespace ash
