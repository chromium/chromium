// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GUEST_VIEW_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GUEST_VIEW_POLICY_H_

namespace content {
class WebContents;
}

namespace performance_manager {

// Notifies Performance Manager that a guest view has been associated with a
// WebContents.
void GuestViewAssociatedToWebContents(content::WebContents* guest_web_contents);

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GUEST_VIEW_POLICY_H_
