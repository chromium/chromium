// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_

#include <string>

#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
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
  static void UpdateDataProtectionState(
      content::Page& page,
      const std::string& watermark_text,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  ~DataProtectionPageUserData() override;

  void set_watermark_text(const std::string& watermark_text) {
    watermark_text_ = watermark_text;
  }
  const std::string& watermark_text() { return watermark_text_; }

  void set_rt_lookup_response(
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
    rt_lookup_response_ = std::move(rt_lookup_response);
  }
  const safe_browsing::RTLookupResponse* rt_lookup_response() {
    return rt_lookup_response_.get();
  }

 private:
  friend class content::PageUserData<DataProtectionPageUserData>;

  DataProtectionPageUserData(
      content::Page& page,
      const std::string& watermark_text,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  std::string watermark_text_;
  std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response_;

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
