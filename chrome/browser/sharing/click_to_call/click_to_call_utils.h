// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_

#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Returns true if click to call feature should be shown for |url|.
bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url);

// Returns the first possible phone number in |selected_text| if click to call
// should be offered. Otherwise returns base::nullopt.
base::Optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selected_text);

// Unescapes and returns the URL contents.
std::string GetUnescapedURLContent(const GURL& url);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
