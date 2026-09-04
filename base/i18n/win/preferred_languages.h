// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_WIN_PREFERRED_LANGUAGES_H_
#define BASE_I18N_WIN_PREFERRED_LANGUAGES_H_

#include <vector>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"

namespace base::i18n {

// Returns the list of user-preferred UI languages from MUI, if available,
// falling back to the user-default UI language otherwise.
BASE_I18N_EXPORT std::vector<LanguageTag> GetUserPreferredUILanguageList();

// Returns the list of thread-, process-, user-, and system-preferred UI
// languages.
BASE_I18N_EXPORT std::vector<LanguageTag> GetThreadPreferredUILanguageList();

}  // namespace base::i18n

#endif  // BASE_I18N_WIN_PREFERRED_LANGUAGES_H_
