// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offliner_user_data.h"

#include <memory>

namespace offline_pages {

void OfflinerUserData::AddToWebContents(content::WebContents* webcontents,
                                        BackgroundLoaderOffliner* offliner) {
  DCHECK(offliner);
  webcontents->SetUserData(UserDataKey(),
                           std::make_unique<OfflinerUserData>(offliner));
}

// static - gets the data pointer as a BackgroundLoaderOffliner
BackgroundLoaderOffliner* OfflinerUserData::OfflinerFromWebContents(
    content::WebContents* webcontents) {
  OfflinerUserData* data = OfflinerUserData::FromWebContents(webcontents);
  if (data)
    return data->offliner();

  return nullptr;
}

// static - gets the data pointer as a ResourceLoadingObserver
ResourceLoadingObserver*
OfflinerUserData::ResourceLoadingObserverFromWebContents(
    content::WebContents* webcontents) {
  OfflinerUserData* data = OfflinerUserData::FromWebContents(webcontents);
  if (data)
    return data->offliner();

  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfflinerUserData);

}  // namespace offline_pages
