// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <string>

#include "base/strings/string_util.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace {
const char kBookmarkFolderPath[] = "folder/";
}

namespace chrome {
namespace android {

bool HandleAndroidNativePageURL(GURL* url,
                                content::BrowserContext* browser_context) {
  if (url->SchemeIs(content::kChromeUIScheme)) {
    if (url->host() == chrome::kChromeUINewTabHost) {
      *url = GURL(chrome::kChromeUINativeNewTabURL);
      return true;
    }
  }

  if (url->SchemeIs(chrome::kChromeNativeScheme) &&
      url->host() == kChromeUIBookmarksHost) {
    std::string ref = url->ref();
    if (!ref.empty()) {
      *url = GURL(std::string(kChromeUINativeBookmarksURL)
                      .append(kBookmarkFolderPath)
                      .append(ref));
      return true;
    }
  }

  return false;
}

}  // namespace android
}  // namespace chrome
