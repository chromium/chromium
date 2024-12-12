// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GUEST_UTIL_H_
#define CHROME_BROWSER_GLIC_GUEST_UTIL_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace glic {

GURL GetGuestURL();
url::Origin GetGuestOrigin();

// If `guest_contents` is the glic guest, do glic-specific setup and return
// true, otherwise return false.
bool OnGuestAdded(content::WebContents* guest_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GUEST_UTIL_H_
