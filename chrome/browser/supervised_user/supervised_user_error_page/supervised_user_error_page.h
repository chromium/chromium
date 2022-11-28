// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_

#include <string>

namespace supervised_user_error_page {

enum FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  DENYLIST = 2,
  MANUAL = 3,
  ALLOWLIST = 4,
  NOT_SIGNED_IN = 5,
};

int GetBlockMessageID(
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool single_parent);

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const std::string& custodian,
                      const std::string& custodian_email,
                      const std::string& second_custodian,
                      const std::string& second_custodian_email,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale,
                      bool already_sent_remote_request,
                      bool is_main_frame);

}  //  namespace supervised_user_error_page

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_SUPERVISED_USER_ERROR_PAGE_H_
