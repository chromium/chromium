// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_

#include <string>

namespace supervised_user_error_page {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.superviseduser.supervisedusererrorpage)
enum FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  BLACKLIST = 2,
  MANUAL = 3,
  WHITELIST = 4,
  NOT_SIGNED_IN = 5,
};

int GetBlockMessageID(
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool is_child_account,
    bool single_parent);

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const std::string& custodian,
                      const std::string& custodian_email,
                      const std::string& second_custodian,
                      const std::string& second_custodian_email,
                      bool is_child_account,
                      bool is_deprecated,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale);

}  //  namespace supervised_user_error_page

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
