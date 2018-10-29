// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_INFOBAR_DELEGATE_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}  // namespace content

// An InfoBar delegate for page load capping. This sets up the correct
// strings for both the InfoBar that allows the user to pause the resource
// loading and the InfoBar that allows the user to resume resource loading. When
// the button in the pause InfoBar is clicked, the Resume InfoBar is shown and
// the resource loading is paused. When the button in the resume InfoBar is
// clicked, the resume InfoBar is dismissed, and resources continue to load.
//
// Page load capping is a feature that informs users when a page goes beyond a
// certain amount of network bytes and presents the user an option to pause
// resource loading on the page until the user chooses to resume resource
// loading.
//
// This class cannot be created directly, but an instance can be created using
// Create().
class PageLoadCappingInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // A callback that triggers the page to have its subresource loading paused or
  // resumed based on |pause|.
  using PauseCallback = base::RepeatingCallback<void(bool pause)>;

  // A callback used to get the earliest possible time (offset from now) that
  // the InfoBar could be dismissed based on lack of network usage.
  // |time_to_expire| must be passed in as TimeDelta initialized to 0 to handle
  // the case of the underlying weak pointer being destroyed.
  using TimeToExpireCallback =
      base::RepeatingCallback<void(base::TimeDelta* time_to_expire)>;

  // Creates an InfoBar for page load capping. Returns whether the InfoBar was
  // created. |web_contents| is the WebContents that caused the data usage.
  // |pause_callback| is used to pause and unpause the resource loading of the
  // page. |time_to_expire_callback| is used to get the earliest time at which
  // the page is considered to have stopped using data.
  static bool Create(content::WebContents* web_contents,
                     const PauseCallback& pause_callback,
                     const TimeToExpireCallback& time_to_expire_callback);

  ~PageLoadCappingInfoBarDelegate() override;

  // Used to record UMA on user interaction with the capping heavy pages
  // InfoBar.
  enum class InfoBarInteraction {
    kShowedInfoBar = 0,
    kPausedPage = 1,
    kResumedPage = 2,
    kDismissedByNetworkStopped = 3,
    kMaxValue = kDismissedByNetworkStopped,
  };

 protected:
  PageLoadCappingInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetButtons() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override = 0;
  bool LinkClicked(WindowOpenDisposition disposition) override = 0;
  base::string16 GetLinkText() const override = 0;

  DISALLOW_COPY_AND_ASSIGN(PageLoadCappingInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_INFOBAR_DELEGATE_H_
