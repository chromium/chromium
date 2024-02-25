// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_

#include <optional>
#include <string>

#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Returns true if click to call feature should be shown for |url|.
bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url);

// Returns the first possible phone number in |selection_text| if click to call
// should be offered. Otherwise returns std::nullopt.
std::optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text);

// Checks if the given |url| is safe to be used by Click to Call to be sent to
// remote Android devices. Note that the remote device might open the dialer
// immediately with the given |url| so any USSD codes should return false here.
bool IsUrlSafeForClickToCall(const GURL& url);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
