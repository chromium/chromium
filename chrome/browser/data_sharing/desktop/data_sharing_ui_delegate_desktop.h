// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_UI_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_UI_DELEGATE_DESKTOP_H_

#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"

namespace data_sharing {

// Desktop implementation of DataSharingUIDelegate.
class DataSharingUIDelegateDesktop : public DataSharingUIDelegate {
 public:
  explicit DataSharingUIDelegateDesktop(Profile* profile);
  ~DataSharingUIDelegateDesktop() override;

  // DataSharingUIDelegate implementation.
  void HandleShareURLIntercepted(const GURL& url) override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_UI_DELEGATE_DESKTOP_H_
