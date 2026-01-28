// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace android {

// Defines an extensions-free implementation of the GuestViewManagerDelegate
// interface that knows the concept of a task manager and the need for tagging
// the guest view WebContents by their appropriate task manager tag.
class ChromeGuestViewManagerDelegate
    : public guest_view::GuestViewManagerDelegate {
 public:
  ChromeGuestViewManagerDelegate();
  ~ChromeGuestViewManagerDelegate() override;

 private:
  // GuestViewManagerDelegate:
  void OnGuestAdded(content::WebContents* guest_web_contents) const override;

  void DispatchEvent(const std::string& event_name,
                     base::DictValue args,
                     guest_view::GuestViewBase* guest,
                     int instance_id) override;

  bool IsGuestAvailableToContext(
      const guest_view::GuestViewBase* guest) const override;

  bool IsOwnedByExtension(const guest_view::GuestViewBase* guest) override;

  bool IsOwnedByControlledFrameEmbedder(
      const guest_view::GuestViewBase* guest) override;

  void RegisterAdditionalGuestViewTypes(
      guest_view::GuestViewManager* manager) override;
};
}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_GUEST_VIEW_CHROME_GUEST_VIEW_MANAGER_DELEGATE_H_
