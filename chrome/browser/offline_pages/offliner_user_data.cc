// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offliner_user_data.h"

#include <memory>

namespace offline_pages {

OfflinerUserData::OfflinerUserData(content::WebContents* web_contents,
                                   BackgroundLoaderOffliner* offliner)
    : content::WebContentsUserData<OfflinerUserData>(*web_contents),
      offliner_(offliner) {}

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
