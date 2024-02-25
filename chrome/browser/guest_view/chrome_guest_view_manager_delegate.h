// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

namespace extensions {

// Defines a chrome-specific implementation of
// ExtensionsGuestViewManagerDelegate that knows about the concept of a task
// manager and the need for tagging the guest view WebContents by their
// appropriate task manager tag.
class ChromeGuestViewManagerDelegate
    : public ExtensionsGuestViewManagerDelegate {
 public:
  ChromeGuestViewManagerDelegate();

  ChromeGuestViewManagerDelegate(const ChromeGuestViewManagerDelegate&) =
      delete;
  ChromeGuestViewManagerDelegate& operator=(
      const ChromeGuestViewManagerDelegate&) = delete;

  ~ChromeGuestViewManagerDelegate() override;

  // GuestViewManagerDelegate:
  void OnGuestAdded(content::WebContents* guest_web_contents) const override;
  bool IsOwnedByControlledFrameEmbedder(
      const guest_view::GuestViewBase* guest) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_
