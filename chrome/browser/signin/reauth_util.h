// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_REAUTH_UTIL_H_
#define CHROME_BROWSER_SIGNIN_REAUTH_UTIL_H_

#include "components/signin/public/base/signin_metrics.h"
#include "url/gurl.h"

namespace signin {

// Returns a URL to display in the reauth confirmation dialog. The dialog was
// triggered by |access_point|.
GURL GetReauthConfirmationURL(signin_metrics::ReauthAccessPoint access_point);

// Returns ReauthAccessPoint encoded in the query of the reauth confirmation
// URL.
signin_metrics::ReauthAccessPoint GetReauthAccessPointForReauthConfirmationURL(
    const GURL& url);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_REAUTH_UTIL_H_
