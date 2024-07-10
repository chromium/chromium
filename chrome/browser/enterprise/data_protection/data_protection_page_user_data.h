// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_

#include <string>

#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "content/public/browser/page_user_data.h"

namespace enterprise_data_protection {

// A structure holding all data protection settings for a given URL.
struct UrlSettings {
  // The watermark text that should apply to tabs showing this URL.  An empty
  // string means no watermark should be shown.
  std::string watermark_text;

  bool allow_screenshots = true;

  bool operator==(const UrlSettings& other) const;

  // URL settings that imply no data protections are enabled.
  static const UrlSettings& None();
};

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
  // Sets the RT URL lookup response for the page of the WebContents' primary
  // main RFH.  During navigations this should only be called after the page is
  // ready to be committed, otherwise the state will be saved to an intermediate
  // Page.
  static void UpdateRTLookupResponse(
      content::Page& page,
      const std::string& identifier,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  // Sets whether screenshots are allowed for the page of the WebContents'
  // primary main RFH.  During navigations this should only be called after the
  // page is ready to be committed, otherwise the state will be saved to an
  // intermediate Page.
  static void UpdateDataControlsScreenshotState(content::Page& page,
                                                const std::string& identifier,
                                                bool allow);

  ~DataProtectionPageUserData() override;

  // This function will return a `UrlSettings` object representing a combination
  // of the restrictions applied by a `RTLookupResponse` if one exists and by
  // Data Controls rules.
  UrlSettings settings() const;

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
      const std::string& identifier,
      UrlSettings settings,
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  // There are two sources for data protection settings as of this writing: data
  // controls and URL filtering. data_controls_settings_, as the name suggests,
  // will store data protection settings obtained from data controls. The URL
  // filtering Data Protection settings are implicitly stored in
  // rt_lookup_response_.
  std::string identifier_;
  UrlSettings data_controls_settings_;
  std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response_;

  PAGE_USER_DATA_KEY_DECL();
};

// Return the watermark string to display if present in `threat_info`. Revealed
// for testing
std::string GetWatermarkString(
    const std::string& identifier,
    const safe_browsing::MatchedUrlNavigationRule& rule);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_PAGE_USER_DATA_H_
