// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

// Uses the SharingService to fetch an SMS from a remote device.
void FetchRemoteSms(content::BrowserContext* context,
                    const url::Origin& origin,
                    base::OnceCallback<void(base::Optional<std::string>)>);

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_
