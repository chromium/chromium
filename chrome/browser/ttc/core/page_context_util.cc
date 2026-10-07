// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context_util.h"

#include <string_view>

#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ttc {

bool IsUrlSupportedForPageContext(const GURL& url) {
  // Similar to, but stricter than, Glic's IsTabValidForSharing() (see
  // chrome/browser/glic/host/context/glic_sharing_utils.cc), which also allows
  // file: URLs and a few other WebUI pages.
  // TODO(b/555804152): On Android the New Tab Page is native
  // (chrome-native://newtab/) and so is not supported here. Decide whether it
  // should be.
  if (url.SchemeIs(content::kChromeUIScheme)) {
    const std::string_view host = url.host();
    if (host == chrome::kChromeUINewTabHost ||
        host == chrome::kChromeUINewTabPageHost ||
        host == chrome::kChromeUINewTabPageThirdPartyHost) {
      return true;
    }
  }
  // Excludes all other chrome:// URLs, as well as file:, javascript:, about:,
  // data:, blob:, chrome-extension:, etc.
  return url.SchemeIsHTTPOrHTTPS();
}

}  // namespace ttc
