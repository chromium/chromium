// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UTILS_H_

#include "url/gurl.h"

namespace enterprise_data_protection {

// Returns the URL that data protection checks should apply to for `url`. If
// `url` is a distilled page URL (chrome-distiller://), the original URL of the
// distilled page is returned so that policies apply to the page the content
// actually comes from. `url` is returned as-is otherwise.
GURL GetOriginalUrl(const GURL& url);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UTILS_H_
