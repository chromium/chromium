// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ANDROID_LOCALE_H_
#define BASE_I18N_ANDROID_LOCALE_H_

#include <string>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"

namespace base::i18n {

// Return the current default country code of the device as a string.
BASE_I18N_EXPORT std::string GetAndroidDefaultCountryCode();

// Return the current default locale of the device as a LanguageTag.
BASE_I18N_EXPORT LanguageTag GetAndroidDefaultLocale();

}  // namespace base::i18n

#endif  // BASE_I18N_ANDROID_LOCALE_H_
