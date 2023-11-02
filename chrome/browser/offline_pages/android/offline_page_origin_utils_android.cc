// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_origin_utils.h"

#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "content/public/browser/web_contents.h"

namespace offline_pages {
// static
std::string OfflinePageOriginUtils::GetEncodedOriginAppFor(
    content::WebContents* web_contents) {
  return android::OfflinePageBridge::GetEncodedOriginApp(web_contents);
}
}  // namespace offline_pages
