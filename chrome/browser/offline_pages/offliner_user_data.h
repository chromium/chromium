// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_USER_DATA_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_USER_DATA_H_

#include "chrome/browser/offline_pages/background_loader_offliner.h"
#include "chrome/browser/offline_pages/resource_loading_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {

class OfflinerUserData : public content::WebContentsUserData<OfflinerUserData> {
 public:
  static void AddToWebContents(content::WebContents* webcontents,
                               BackgroundLoaderOffliner* offliner);

  static BackgroundLoaderOffliner* OfflinerFromWebContents(
      content::WebContents* webcontents);

  static ResourceLoadingObserver* ResourceLoadingObserverFromWebContents(
      content::WebContents* webcontents);

  explicit OfflinerUserData(BackgroundLoaderOffliner* offliner) {
    offliner_ = offliner;
  }
  BackgroundLoaderOffliner* offliner() { return offliner_; }

 private:
  friend class content::WebContentsUserData<OfflinerUserData>;
  // The offliner that the WebContents is attached to. The offliner owns the
  // Delegate which owns the WebContents that this data is attached to.
  // Therefore, its lifetime should exceed that of the WebContents, so this
  // should always be non-null.
  BackgroundLoaderOffliner* offliner_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_USER_DATA_H_
