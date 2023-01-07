// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_common.h"

namespace profiles {
const char kProfileStatisticsBrowsingHistory[] = "BrowsingHistory";
const char kProfileStatisticsPasswords[] = "Passwords";
const char kProfileStatisticsBookmarks[] = "Bookmarks";
const char kProfileStatisticsAutofill[] = "Autofill";

const std::array<const char*, 4> kProfileStatisticsCategories = {
    {kProfileStatisticsBrowsingHistory, kProfileStatisticsPasswords,
     kProfileStatisticsBookmarks, kProfileStatisticsAutofill}};

}  // namespace profiles
