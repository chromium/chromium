// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_

#include "base/feature_list.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace glic {

BASE_DECLARE_FEATURE(kGlicGuestUrlMultiInstanceParam);

// Returns the URL/origin from where the guest web client will be loaded from.
GURL GetGuestURL();
url::Origin GetGuestOrigin();

// Returns an updated guest URL that includes a language parameter, set to the
// browser's UI language. If the parameter is already present, its current value
// will not be changed.
GURL GetLocalizedGuestURL(const GURL& guest_url);

// If multi-instance is enabled return the guest_url with the multi-instance
// parameter added. Otherwise return the guest_url unchanged.
GURL MaybeAddMultiInstanceParameter(const GURL& guest_url);

// Returns true if `web_contents` contains the Glic WebUI application.
bool IsGlicWebUI(const content::WebContents* web_contents);

// If `guest_contents` is the glic guest, do glic-specific setup and return
// true, otherwise return false.
bool OnGuestAdded(content::WebContents* guest_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_H_
