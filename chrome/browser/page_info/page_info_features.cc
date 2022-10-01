// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/page_info_features.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/page_info/core/features.h"

namespace page_info {

bool IsMoreAboutThisSiteFeatureEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  // MoreAbout may only be enabled when the side panel is on.
  if (!base::FeatureList::IsEnabled(features::kUnifiedSidePanel))
    return false;
#endif

  if (!page_info::IsAboutThisSiteFeatureEnabled(
          g_browser_process->GetApplicationLocale())) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      page_info::kPageInfoAboutThisSiteMoreInfo);
}

bool IsDescriptionPlaceholderFeatureEnabled() {
  return IsMoreAboutThisSiteFeatureEnabled() &&
         base::FeatureList::IsEnabled(
             page_info::kPageInfoAboutThisSiteDescriptionPlaceholder);
}

#if !BUILDFLAG(IS_ANDROID)
bool IsPersistentSidePanelEntryFeatureEnabled() {
  return IsMoreAboutThisSiteFeatureEnabled() &&
         base::FeatureList::IsEnabled(
             page_info::kAboutThisSitePersistentSidePanelEntry);
}

BASE_FEATURE(kAboutThisSitePersistentSidePanelEntry,
             "AboutThisSitePersistentSidePanelEntry",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace page_info
