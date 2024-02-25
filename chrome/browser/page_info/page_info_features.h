// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_
#define CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_

#include "base/feature_list.h"
namespace page_info {

// Returns true if kPageInfoAboutThisSiteMoreInfo and dependent features are
// enabled.
bool IsAboutThisSiteFeatureEnabled();

// Returns true if kAboutThisSiteAsyncFetching and dependent features are
// enabled.
bool IsAboutThisSiteAsyncFetchingEnabled();

// Enables usage of the async fetching method for cacao and caching fetched
// metadata in a TabHelper.
BASE_DECLARE_FEATURE(kAboutThisSiteAsyncFetching);

#if !BUILDFLAG(IS_ANDROID)
// Returns true if kAboutThisSitePersistentSidePanelEntry and dependent
// features are enabled.
bool IsPersistentSidePanelEntryFeatureEnabled();

// Enables the persistent "About this site" entry in the side panel.
BASE_DECLARE_FEATURE(kAboutThisSitePersistentSidePanelEntry);

#endif

}  // namespace page_info

#endif  // CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_
