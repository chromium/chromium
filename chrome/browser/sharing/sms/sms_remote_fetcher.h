// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace content {
class WebContents;
enum class SmsFetchFailureType;
}

namespace url {
class Origin;
}

// Uses the SmsRemoteFetcherUiCOntroller to fetch an SMS from a remote device.
// Returns a callback that cancels receiving of the response. Calling it will
// clear the sharing states including the UI element in the omnibox. If the
// response has been received already, running the callback will be a no-op.
// Returns a null callback if fetching from a remote device is disabled.
base::OnceClosure FetchRemoteSms(
    content::WebContents* web_contents,
    const std::vector<url::Origin>& origin_list,
    base::OnceCallback<void(std::optional<std::vector<url::Origin>>,
                            std::optional<std::string>,
                            std::optional<content::SmsFetchFailureType>)>);

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_H_
