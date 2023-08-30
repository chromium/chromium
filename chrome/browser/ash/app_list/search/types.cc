// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/types.h"

#include <stddef.h>
#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_service.h"

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

bool IsControlCategoryEnabled(const Profile* profile,
                              const ControlCategory control_category) {
  const std::string pref_name =
      ash::GetAppListControlCategoryName(control_category);
  // An empty pref_name indicates it is non-toggleable and always enabled.
  if (pref_name.empty()) {
    return true;
  }

  return profile->GetPrefs()
      ->GetDict(ash::prefs::kLauncherSearchCategoryControlStatus)
      .FindBool(pref_name)
      .value_or(true);
}

}  // namespace app_list
