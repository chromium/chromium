// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <memory>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/util/type_safety/pass_key.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using password_manager::CredentialPair;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

using IsPublicSuffixMatch = CredentialPair::IsPublicSuffixMatch;

constexpr char kExampleCom[] = "https://example.com/";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD1(TouchToFillClosed, void(ShowVirtualKeyboard));
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD3(Show,
               void(const GURL&,
                    IsOriginSecure,
                    base::span<const CredentialPair>));
  MOCK_METHOD1(OnCredentialSelected, void(const CredentialPair&));
  MOCK_METHOD0(OnDismiss, void());
};

}  // namespace

class TouchToFillControllerTest : public testing::Test {
 protected:
  using UkmBuilder = ukm::builders::TouchToFill_Shown;

  TouchToFillControllerTest() {
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller_.set_view(std::move(mock_view));

    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  MockTouchToFillView& view() { return *mock_view_; }

  ukm::TestAutoSetUkmRecorder& test_recorder() { return test_recorder_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockTouchToFillView* mock_view_ = nullptr;
  MockPasswordManagerDriver driver_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  TouchToFillController touch_to_fill_controller_{
      util::PassKey<TouchToFillControllerTest>()};
};

TEST_F(TouchToFillControllerTest, Show_And_Fill) {
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the absence of an Android credential.
  base::HistogramTester tester;
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("alice"),
                                       base::ASCIIToUTF16("p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            false, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

TEST_F(TouchToFillControllerTest, Show_Insecure_Origin) {
  EXPECT_CALL(driver(), GetLastCommittedURL())
      .WillOnce(ReturnRefOfCopy(GURL("http://example.com")));

  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(),
              Show(Eq(GURL("http://example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)},
      {base::ASCIIToUTF16("bob"), base::ASCIIToUTF16("s3cr3t"),
       GURL("android://hash@com.example.my"), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the presence of an Android credential.
  base::HistogramTester tester;
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("bob"),
                                       base::ASCIIToUTF16("s3cr3t")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            true, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

TEST_F(TouchToFillControllerTest, Dismiss) {
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(true)));
  touch_to_fill_controller().OnDismiss();

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(TouchToFillController::UserAction::kDismissed));
}
