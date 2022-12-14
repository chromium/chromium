// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/types.h"

#include <stddef.h>

namespace app_list {

CategoriesList CreateAllCategories() {
  CategoriesList res({{.category = Category::kApps},
                      {.category = Category::kAppShortcuts},
                      {.category = Category::kWeb},
                      {.category = Category::kFiles},
                      {.category = Category::kSettings},
                      {.category = Category::kHelp},
                      {.category = Category::kPlayStore},
                      {.category = Category::kSearchAndAssistant},
                      {.category = Category::kGames}});
  DCHECK_EQ(res.size(), static_cast<size_t>(Category::kMaxValue));
  return res;
}

}  // namespace app_list
