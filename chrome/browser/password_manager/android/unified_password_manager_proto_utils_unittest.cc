// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/unified_password_manager_proto_utils.h"

#include "chrome/browser/password_manager/android/protos/list_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/password_info.pb.h"
#include "chrome/browser/password_manager/android/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::Eq;

constexpr time_t kIssuesCreationTime = 1337;
constexpr char kTestOrigin[] = "https://www.origin.com/";
constexpr char kTestAction[] = "https://www.action.com/";
constexpr char kTestFormName[] = "login_form";
const std::u16string kTestFormName16(u"login_form");
constexpr char kTestUsernameElementName[] = "username_element";
const std::u16string kTestUsernameElementName16(u"username_element");
constexpr autofill::FormControlType kTestUsernameElementType =
    autofill::FormControlType::kInputText;
constexpr char kTestPasswordElementName[] = "password_element";
const std::u16string kTestPasswordElementName16(u"password_element");
constexpr autofill::FormControlType kTestPasswordElementType =
    autofill::FormControlType::kInputPassword;

sync_pb::PasswordSpecificsData CreateSpecificsData(
    const std::string& origin,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm) {
  sync_pb::PasswordSpecificsData password_specifics;
  password_specifics.set_origin(origin);
  password_specifics.set_username_element(username_element);
  password_specifics.set_username_value(username_value);
  password_specifics.set_password_element(password_element);
  password_specifics.set_signon_realm(signon_realm);
  password_specifics.set_scheme(static_cast<int>(PasswordForm::Scheme::kHtml));
  password_specifics.set_action(GURL(origin).spec());
  password_specifics.set_password_value("D3f4ultP4$$w0rd");
  password_specifics.set_date_last_used(kIssuesCreationTime);
  password_specifics.set_date_created(kIssuesCreationTime);
  password_specifics.set_date_password_modified_windows_epoch_micros(
      kIssuesCreationTime);
  password_specifics.set_blacklisted(false);
  password_specifics.set_type(
      static_cast<int>(PasswordForm::Type::kFormSubmission));
  password_specifics.set_times_used(1);
  password_specifics.set_display_name("display_name");
  password_specifics.set_avatar_url(GURL(origin).spec());
  password_specifics.set_federation_url(std::string());
  // The current code always populates password issues for outgoing protos even
  // when none exist.
  *password_specifics.mutable_password_issues() = sync_pb::PasswordIssues();
  // The current code always populates notes for outgoing protos even when none
  // exist.
  password_specifics.mutable_notes();
  // The current code always populates shared password metadata for outgoing
  // protos even when none exist.
  password_specifics.set_sender_email("");
  password_specifics.set_sender_name("");
  password_specifics.set_date_received_windows_epoch_micros(0);
  password_specifics.set_sharing_notification_displayed(false);
  password_specifics.set_sender_profile_image_url("");
  return password_specifics;
}

}  // namespace

class UnifiedPasswordManagerProtoUtilsTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::pair<IsAccountStore, PasswordForm::Store>> {};

TEST_P(UnifiedPasswordManagerProtoUtilsTest,
       ConvertPasswordWithLocalDataToFullPasswordFormAndBack) {
  PasswordWithLocalData password_data;
  *password_data.mutable_password_specifics_data() = CreateSpecificsData(
      kTestOrigin, kTestUsernameElementName, "username_value",
      kTestPasswordElementName, "signon_realm");
  (*password_data.mutable_local_data())
      .set_previously_associated_sync_account_email("test@gmail.com");
  const std::string kTestUsernameElementTypeStr(
      autofill::FormControlTypeToString(kTestUsernameElementType));
  const std::string kTestPasswordElementTypeStr(
      autofill::FormControlTypeToString(kTestPasswordElementType));
  std::string opaque_metadata =
      "{\"form_data\":{\"action\":\"" + std::string(kTestAction) +
      "\",\"fields\":[{\"form_control_type\":\"" + kTestUsernameElementTypeStr +
      "\",\"name\":\"" + kTestUsernameElementName +
      "\"},{\"form_control_type\":\"" + kTestPasswordElementTypeStr +
      "\",\"name\":\"" + kTestPasswordElementName + "\"}],\"name\":\"" +
      kTestFormName + "\",\"url\":\"" + kTestOrigin +
      "\"},\"skip_zero_click\":false}";
  (*password_data.mutable_local_data()).set_opaque_metadata(opaque_metadata);

  PasswordForm form = PasswordFromProtoWithLocalData(password_data);
  EXPECT_THAT(form.url, Eq(GURL(kTestOrigin)));
  EXPECT_THAT(form.username_element, Eq(kTestUsernameElementName16));
  EXPECT_THAT(form.username_value, Eq(u"username_value"));
  EXPECT_THAT(form.password_element, Eq(kTestPasswordElementName16));
  EXPECT_THAT(form.signon_realm, Eq("signon_realm"));
  EXPECT_FALSE(form.skip_zero_click);
  EXPECT_EQ(form.form_data.url(), GURL(kTestOrigin));
  EXPECT_EQ(form.form_data.action(), GURL(kTestAction));
  EXPECT_EQ(form.form_data.name(), kTestFormName16);
  ASSERT_EQ(form.form_data.fields().size(), 2u);
  EXPECT_EQ(form.form_data.fields()[0].name(), kTestUsernameElementName16);
  EXPECT_EQ(form.form_data.fields()[0].form_control_type(),
            kTestUsernameElementType);
  EXPECT_EQ(form.form_data.fields()[1].name(), kTestPasswordElementName16);
  EXPECT_EQ(form.form_data.fields()[1].form_control_type(),
            kTestPasswordElementType);

  PasswordWithLocalData password_data_converted_back =
      PasswordWithLocalDataFromPassword(form);
  EXPECT_EQ(password_data.SerializeAsString(),
            password_data_converted_back.SerializeAsString());
}

TEST_P(UnifiedPasswordManagerProtoUtilsTest, ConvertListResultToFormVector) {
  ListPasswordsResult list_result;
  PasswordWithLocalData password1;
  *password1.mutable_password_specifics_data() =
      CreateSpecificsData("http://1.origin.com/", "username_1", "username_1",
                          "password_1", "signon_1");
  PasswordWithLocalData password2;
  *password2.mutable_password_specifics_data() =
      CreateSpecificsData("http://2.origin.com/", "username_2", "username_2",
                          "password_2", "signon_2");
  *list_result.add_password_data() = password1;
  *list_result.add_password_data() = password2;

  std::vector<PasswordForm> forms =
      PasswordVectorFromListResult(list_result, GetParam().first);

  std::vector<PasswordForm> expected_forms = {
      PasswordFromProtoWithLocalData(password1),
      PasswordFromProtoWithLocalData(password2)};
  expected_forms[0].in_store = GetParam().second;
  expected_forms[1].in_store = GetParam().second;

  EXPECT_THAT(forms, testing::ElementsAreArray(expected_forms));
}

TEST_P(UnifiedPasswordManagerProtoUtilsTest,
       ConvertListPasswordsWithUiInfoResultToFormVector) {
  ListPasswordsWithUiInfoResult list_result;
  ListPasswordsWithUiInfoResult::PasswordWithUiInfo password1;
  *password1.mutable_password_data()->mutable_password_specifics_data() =
      CreateSpecificsData("http://1.origin.com/", "username_1", "username_1",
                          "password_1", "signon_1");
  PasswordInfo ui_info;
  ui_info.set_display_name("Example app");
  ui_info.set_icon_url("http://example.com/favicon.ico");
  *password1.mutable_ui_info() = ui_info;
  ListPasswordsWithUiInfoResult::PasswordWithUiInfo password2;
  *password2.mutable_password_data()->mutable_password_specifics_data() =
      CreateSpecificsData("http://2.origin.com/", "username_2", "username_2",
                          "password_2", "signon_2");
  *list_result.add_passwords_with_ui_info() = password1;
  *list_result.add_passwords_with_ui_info() = password2;

  std::vector<PasswordForm> forms =
      PasswordVectorFromListResult(list_result, GetParam().first);
  std::vector<PasswordForm> expected_forms = {
      PasswordFromProtoWithLocalData(password1.password_data()),
      PasswordFromProtoWithLocalData(password2.password_data())};
  expected_forms[0].app_display_name = ui_info.display_name();
  expected_forms[0].app_icon_url = GURL(ui_info.icon_url());
  expected_forms[0].in_store = GetParam().second;
  expected_forms[1].in_store = GetParam().second;

  EXPECT_THAT(forms, testing::ElementsAreArray(expected_forms));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    UnifiedPasswordManagerProtoUtilsTest,
    testing::ValuesIn({std::make_pair(IsAccountStore(true),
                                      PasswordForm::Store::kAccountStore),
                       std::make_pair(IsAccountStore(false),
                                      PasswordForm::Store::kProfileStore)}),
    [](const ::testing::TestParamInfo<
        std::pair<IsAccountStore, PasswordForm::Store>>& info) {
      return info.param.first ? "Account" : "Profile";
    });

}  // namespace password_manager
