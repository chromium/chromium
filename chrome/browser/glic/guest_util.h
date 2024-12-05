// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GUEST_UTIL_H_
#define CHROME_BROWSER_GLIC_GUEST_UTIL_H_

namespace content {
class WebContents;
}

namespace glic {

// If `guest_contents` is the glic guest, do glic-specific setup.
void OnGuestAdded(content::WebContents* guest_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GUEST_UTIL_H_
