// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

#include "base/feature_list.h"

bool IsRecipeTasksModuleEnabled();
bool IsCartModuleEnabled();
bool IsDriveModuleEnabled();
bool IsHistoryClustersModuleEnabled();

bool IsEnUSLocaleOnlyFeatureEnabled(const base::Feature& ntp_feature);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
