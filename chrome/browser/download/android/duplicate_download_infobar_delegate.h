// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace android {

// An infobar that asks if user wants to continue downloading when there is
// already a duplicate file in storage. If user chooses to proceed,
// a new file will be created.
// Note that this infobar does not expire if the user subsequently navigates,
// since such navigations won't automatically cancel the underlying download.
class DuplicateDownloadInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Gets the file path to be downloaded.
  virtual std::string GetFilePath() const = 0;

  // Whether the download is for offline page.
  virtual bool IsOfflinePage() const;

  virtual std::string GetPageURL() const;

  // The OTRProfileID of the download. Null if for regular mode.
  virtual std::optional<Profile::OTRProfileID> GetOTRProfileID() const;

  // Whether the duplicate is an in-progress request or completed download.
  virtual bool DuplicateRequestExists() const;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
};

}  // namespace android

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
