// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/extensions_utils.h"

#include "base/strings/string_util.h"
#include "components/url_matcher/url_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"

namespace supervised_user {

bool IsSupportedChromeExtensionURL(const GURL& effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static const char* const kCrxDownloadUrls[] = {
      "https://clients2.googleusercontent.com/crx/blobs/",
      "https://chrome.google.com/webstore/download/"};

  // Chrome Webstore.
  if (extension_urls::IsWebstoreDomain(
          url_matcher::util::Normalize(effective_url))) {
    return true;
  }

  // Allow webstore crx downloads. This applies to both extension installation
  // and updates.
  if (extension_urls::GetWebstoreUpdateUrl() ==
      url_matcher::util::Normalize(effective_url)) {
    return true;
  }

  // The actual CRX files are downloaded from other URLs. Allow them too.
  // These URLs have https scheme.
  if (!effective_url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  for (const char* crx_download_url_str : kCrxDownloadUrls) {
    GURL crx_download_url(crx_download_url_str);
    if (crx_download_url.host_piece() == effective_url.host_piece() &&
        base::StartsWith(effective_url.path_piece(),
                         crx_download_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

}  // namespace supervised_user
