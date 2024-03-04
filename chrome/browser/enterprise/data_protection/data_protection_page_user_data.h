// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_

#include <string>

#include "content/public/browser/page_user_data.h"

namespace enterprise_data_protection {

// Page user data attached at the end of a WebContents navigation to remember
// the screenshot allow or deny state.  This user data is attached in the
// DidFinishNavigation() step of the navigation.
//
// Note that because of the way Pages are managed by the navigation, this
// user data cannot be accessed before the page is ready to be committed.
// Specifically, this can't be accessed from steps like DidStartNavigation()
// or DidRedirectNavigation().
class DataProtectionPageUserData
    : public content::PageUserData<DataProtectionPageUserData> {
 public:
  // Sets the DataProtection settings for the page of the WebContents' primary
  // main RFH.  During navigations this should only be called after the page is
  // ready to be committed, otherwise the state will be saved to an intermediate
  // Page.
  static void UpdateDataProtectionState(content::Page& page,
                                        const std::string& watermark_text);

  ~DataProtectionPageUserData() override;

  void set_watermark_text(const std::string& watermark_text) {
    watermark_text_ = watermark_text;
  }
  const std::string& watermark_text() { return watermark_text_; }

 private:
  friend class content::PageUserData<DataProtectionPageUserData>;

  DataProtectionPageUserData(content::Page& page,
                             const std::string& watermark_text);

  std::string watermark_text_;

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
