// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ash/app_list/search/types.h"

class Profile;

namespace app_list {

// Returns the absolute path of the directory rankers should serialize their
// state into.
base::FilePath RankerStateDirectory(Profile* profile);

std::string CategoryToString(const Category value);

Category StringToCategory(const std::string& value);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_UTIL_H_
