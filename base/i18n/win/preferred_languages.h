// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_WIN_PREFERRED_LANGUAGES_H_
#define BASE_I18N_WIN_PREFERRED_LANGUAGES_H_

#include <string>
#include <vector>

#include "base/i18n/base_i18n_export.h"

namespace base {
namespace win {
namespace i18n {

// Adds to |languages| the list of user preferred UI languages from MUI, if
// available, falling-back on the user default UI language otherwise.  Returns
// true if at least one language is added.
BASE_I18N_EXPORT bool GetUserPreferredUILanguageList(
    std::vector<std::wstring>* languages);

// Adds to |languages| the list of thread, process, user, and system preferred
// UI languages from MUI, if available, falling-back on the user default UI
// language otherwise.  Returns true if at least one language is added.
BASE_I18N_EXPORT bool GetThreadPreferredUILanguageList(
    std::vector<std::wstring>* languages);

}  // namespace i18n
}  // namespace win
}  // namespace base

#endif  // BASE_I18N_WIN_PREFERRED_LANGUAGES_H_
