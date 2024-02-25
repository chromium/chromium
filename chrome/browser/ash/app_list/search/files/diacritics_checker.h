// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DIACRITICS_CHECKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DIACRITICS_CHECKER_H_

#include <string>

namespace app_list {

// Checks if the given UTF-16 text contains some accented letters.
bool HasDiacritics(const std::u16string& text);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DIACRITICS_CHECKER_H_
