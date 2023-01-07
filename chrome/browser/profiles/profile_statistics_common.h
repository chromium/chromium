// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_

#include <array>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace profiles {
// Constants for the categories in ProfileCategoryStats.
extern const char kProfileStatisticsBrowsingHistory[];
extern const char kProfileStatisticsPasswords[];
extern const char kProfileStatisticsBookmarks[];
extern const char kProfileStatisticsAutofill[];

extern const std::array<const char*, 4> kProfileStatisticsCategories;

// Definition of a single return value of |ProfileStatisticsCallback|.
// The data look like these: {"BrowsingHistory", 912},
// {"Passwords", 71}, {"Bookmarks", 120}, {"Autofill", 200}.
struct ProfileCategoryStat {
  std::string category;
  int count;
};

// Definition of the return value of |ProfileStatisticsCallback|.
using ProfileCategoryStats = std::vector<ProfileCategoryStat>;

// Definition of the callback function. Note that a copy of
// |ProfileCategoryStats| is made each time the callback is called.
using ProfileStatisticsCallback =
    base::RepeatingCallback<void(ProfileCategoryStats)>;
}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_COMMON_H_
