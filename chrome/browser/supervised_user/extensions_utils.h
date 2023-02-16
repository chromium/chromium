
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXTENSIONS_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXTENSIONS_UTILS_H_

#include "url/gurl.h"

namespace supervised_user {

// Returns true if both the extensions are enabled and the provived
// url is a Webstore or Download url.
bool IsSupportedChromeExtensionURL(const GURL& effective_url);

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXTENSIONS_UTILS_H_
