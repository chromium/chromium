// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {
namespace android {

OptimizationGuideTabUrlProviderAndroid::OptimizationGuideTabUrlProviderAndroid(
    Profile* profile)
    : OptimizationGuideTabUrlProvider(profile) {}
OptimizationGuideTabUrlProviderAndroid::
    ~OptimizationGuideTabUrlProviderAndroid() = default;

const std::vector<content::WebContents*>
OptimizationGuideTabUrlProviderAndroid::GetAllWebContentsForProfile(
    Profile* profile) {
  std::vector<content::WebContents*> web_contents_list;
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile)
      continue;

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents)
        web_contents_list.push_back(web_contents);
    }
  }
  return web_contents_list;
}

}  // namespace android
}  // namespace optimization_guide
