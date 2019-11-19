// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace supervised_user_error_page {

struct BlockMessageIDTestParameter {
  FilteringBehaviorReason reason;
  bool is_child_account;
  bool single_parent;
  int expected_result;
};

class SupervisedUserErrorPageTest_GetBlockMessageID
    : public ::testing::TestWithParam<BlockMessageIDTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_GetBlockMessageID, GetBlockMessageID) {
  BlockMessageIDTestParameter param = GetParam();
  EXPECT_EQ(param.expected_result,
            GetBlockMessageID(param.reason, param.is_child_account,
                              param.single_parent))
      << "reason = " << param.reason
      << " is_child_account = " << param.is_child_account
      << " single parent = " << param.single_parent;
}

BlockMessageIDTestParameter block_message_id_test_params[] = {
    {DEFAULT, false, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_DEFAULT},
    {DEFAULT, false, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_DEFAULT},
    {DEFAULT, true, true, IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT},
    {DEFAULT, true, false, IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT},
    // SafeSites is not enabled for supervised users.
    {ASYNC_CHECKER, true, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {ASYNC_CHECKER, true, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {MANUAL, false, false, IDS_SUPERVISED_USER_BLOCK_MESSAGE_MANUAL},
    {MANUAL, false, true, IDS_SUPERVISED_USER_BLOCK_MESSAGE_MANUAL},
    {MANUAL, true, true, IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT},
    {MANUAL, true, false, IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT},
};

INSTANTIATE_TEST_SUITE_P(GetBlockMessageIDParameterized,
                         SupervisedUserErrorPageTest_GetBlockMessageID,
                         ::testing::ValuesIn(block_message_id_test_params));

struct BuildHtmlTestParameter {
  bool allow_access_requests;
  const std::string& profile_image_url;
  const std::string& profile_image_url2;
  const std::string& custodian;
  const std::string& custodian_email;
  const std::string& second_custodian;
  const std::string& second_custodian_email;
  bool is_child_account;
  bool is_deprecated;
  FilteringBehaviorReason reason;
  bool has_two_parents;
};

class SupervisedUserErrorPageTest_BuildHtml
    : public ::testing::TestWithParam<BuildHtmlTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_BuildHtml, BuildHtml) {
  BuildHtmlTestParameter param = GetParam();
  std::string result = BuildHtml(
      param.allow_access_requests, param.profile_image_url,
      param.profile_image_url2, param.custodian, param.custodian_email,
      param.second_custodian, param.second_custodian_email,
      param.is_child_account, param.is_deprecated, param.reason, "");
  // The result should contain the original HTML (with $i18n{} replacements)
  // plus scripts that plug values into it. The test can't easily check that the
  // scripts are correct, but can check that the output contains the expected
  // values.
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url));
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url2));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian_email));
  if (param.has_two_parents) {
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian));
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian_email));
  }
  if (param.reason == ASYNC_CHECKER || param.reason == BLACKLIST) {
    EXPECT_THAT(result, testing::HasSubstr("\"showFeedbackLink\":true"));
  } else {
    EXPECT_THAT(result, testing::HasSubstr("\"showFeedbackLink\":false"));
  }

  // Messages containing parameters aren't tested since they get modified before
  // they are added to the result.
  if (param.allow_access_requests) {
    if (param.is_child_account) {
      EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                              IDS_CHILD_BLOCK_INTERSTITIAL_HEADER)));
      EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                              IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED))));
    } else {
      EXPECT_THAT(result,
                  testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                      IDS_CHILD_BLOCK_INTERSTITIAL_HEADER))));
      EXPECT_THAT(result,
                  testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                      IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE))));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED))));
    }
  } else {
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_HEADER))));
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE))));
    EXPECT_THAT(result,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED)));
  }
  if (param.is_child_account) {
    if (param.has_two_parents) {
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT)));
    } else {
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT))));
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT)));
      EXPECT_THAT(
          result,
          testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT))));
    }
  } else {
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT))));
    EXPECT_THAT(
        result,
        testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT))));
  }
}

BuildHtmlTestParameter build_html_test_parameter[] = {
    {true, "url1", "url2", "custodian", "custodian_email", "", "", true, false,
     DEFAULT, false},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", true, false, DEFAULT, true},
    {false, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", true, false, DEFAULT, true},
    {false, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", false, true, DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", false, false, DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", true, false, ASYNC_CHECKER, true},
};

INSTANTIATE_TEST_SUITE_P(GetBlockMessageIDParameterized,
                         SupervisedUserErrorPageTest_BuildHtml,
                         ::testing::ValuesIn(build_html_test_parameter));

}  //  namespace supervised_user_error_page
