// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include <memory>
#include <tuple>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ShowVirtualKeyboard;
using password_manager::UiCredential;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

using IsPublicSuffixMatch = UiCredential::IsPublicSuffixMatch;
using IsAffiliationBasedMatch = UiCredential::IsAffiliationBasedMatch;

constexpr char kExampleCom[] = "https://example.com/";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD2(FillSuggestion,
               void(const std::u16string&, const std::u16string&));
  MOCK_METHOD1(TouchToFillClosed, void(ShowVirtualKeyboard));
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD3(Show,
               void(const GURL&,
                    IsOriginSecure,
                    base::span<const UiCredential>));
  MOCK_METHOD1(OnCredentialSelected, void(const UiCredential&));
  MOCK_METHOD0(OnDismiss, void());
};

struct MakeUiCredentialParams {
  base::StringPiece username;
  base::StringPiece password;
  base::StringPiece origin = kExampleCom;
  bool is_public_suffix_match = false;
  bool is_affiliation_based_match = false;
  base::TimeDelta time_since_last_use;
};

UiCredential MakeUiCredential(MakeUiCredentialParams params) {
  return UiCredential(
      base::UTF8ToUTF16(params.username), base::UTF8ToUTF16(params.password),
      url::Origin::Create(GURL(params.origin)),
      IsPublicSuffixMatch(params.is_public_suffix_match),
      IsAffiliationBasedMatch(params.is_affiliation_based_match),
      base::Time::Now() - params.time_since_last_use);
}

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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockTouchToFillView* mock_view_ = nullptr;
  MockPasswordManagerDriver driver_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  TouchToFillController touch_to_fill_controller_{
      base::PassKey<TouchToFillControllerTest>()};
};

TEST_F(TouchToFillControllerTest, Show_And_Fill) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the absence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("alice"),
                                       base::ASCIIToUTF16("p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", false, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

TEST_F(TouchToFillControllerTest, Show_Empty) {
  EXPECT_CALL(view(), Show).Times(0);
  touch_to_fill_controller().Show({}, driver().AsWeakPtr());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 0, 1);
}

TEST_F(TouchToFillControllerTest, Show_Insecure_Origin) {
  EXPECT_CALL(driver(), GetLastCommittedURL())
      .WillOnce(ReturnRefOfCopy(GURL("http://example.com")));

  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

  EXPECT_CALL(view(),
              Show(Eq(GURL("http://example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  UiCredential credentials[] = {
      MakeUiCredential({
          .username = "alice",
          .password = "p4ssw0rd",
          .time_since_last_use = base::TimeDelta::FromMinutes(2),
      }),
      MakeUiCredential({
          .username = "bob",
          .password = "s3cr3t",
          .origin = "",
          .is_affiliation_based_match = true,
          .time_since_last_use = base::TimeDelta::FromMinutes(3),
      }),
  };

  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the presence of an Android credential.
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("bob"),
                                       base::ASCIIToUTF16("s3cr3t")));
  EXPECT_CALL(driver(), TouchToFillClosed(ShowVirtualKeyboard(false)));
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.TouchToFill.NumCredentialsShown", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.FilledCredentialWasFromAndroidApp", true, 1);

  auto entries = test_recorder().GetEntriesByName(UkmBuilder::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_recorder().ExpectEntryMetric(
      entries[0], UkmBuilder::kUserActionName,
      static_cast<int64_t>(
          TouchToFillController::UserAction::kSelectedCredential));
}

// Verify that the credentials are ordered by their PSL match bit and last
// time used before being passed to the view.
TEST_F(TouchToFillControllerTest, Show_Orders_Credentials) {
  auto alice = MakeUiCredential({
      .username = "alice",
      .password = "p4ssw0rd",
      .time_since_last_use = base::TimeDelta::FromMinutes(3),
  });
  auto bob = MakeUiCredential({
      .username = "bob",
      .password = "s3cr3t",
      .is_public_suffix_match = true,
      .time_since_last_use = base::TimeDelta::FromMinutes(1),
  });
  auto charlie = MakeUiCredential({
      .username = "charlie",
      .password = "very_s3cr3t",
      .time_since_last_use = base::TimeDelta::FromMinutes(2),
  });
  auto david = MakeUiCredential({
      .username = "david",
      .password = "even_more_s3cr3t",
      .is_public_suffix_match = true,
      .time_since_last_use = base::TimeDelta::FromMinutes(4),
  });

  UiCredential credentials[] = {alice, bob, charlie, david};
  EXPECT_CALL(view(), Show(Eq(GURL(kExampleCom)), IsOriginSecure(true),
                           testing::ElementsAre(charlie, alice, bob, david)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Dismiss) {
  UiCredential credentials[] = {
      MakeUiCredential({.username = "alice", .password = "p4ssw0rd"})};

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
