// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_utils.h"

#include "base/logging.h"
#include "components/dom_distiller/core/url_utils.h"
#include "url/gurl.h"

namespace enterprise_data_protection {

GURL GetOriginalUrl(const GURL& url) {
  if (GURL original_url =
          dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url);
      original_url.is_valid()) {
    return original_url;
  } else {
    VLOG(1) << __func__ << " got a invalid url: " << original_url;
  }
  return url;
}

}  // namespace enterprise_data_protection
