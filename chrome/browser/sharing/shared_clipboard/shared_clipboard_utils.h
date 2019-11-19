// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UTILS_H_

#include "base/strings/string16.h"

namespace content {
class BrowserContext;
}  // namespace content

bool ShouldOfferSharedClipboard(content::BrowserContext* browser_context,
                                const base::string16& text);

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_UTILS_H_
