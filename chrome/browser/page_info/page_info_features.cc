// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/page_info_features.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/page_info/core/features.h"

namespace page_info {

bool IsAboutThisSiteFeatureEnabled() {
  return page_info::IsAboutThisSiteFeatureEnabled(
      g_browser_process->GetApplicationLocale());
}

bool IsAboutThisSiteAsyncFetchingEnabled() {
  return IsAboutThisSiteFeatureEnabled() &&
         base::FeatureList::IsEnabled(kAboutThisSiteAsyncFetching);
}

BASE_FEATURE(kAboutThisSiteAsyncFetching,
             "AboutThisSiteAsyncFetching",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
bool IsPersistentSidePanelEntryFeatureEnabled() {
  return IsAboutThisSiteFeatureEnabled() &&
         base::FeatureList::IsEnabled(
             page_info::kAboutThisSitePersistentSidePanelEntry);
}

BASE_FEATURE(kAboutThisSitePersistentSidePanelEntry,
             "AboutThisSitePersistentSidePanelEntry",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif

}  // namespace page_info
