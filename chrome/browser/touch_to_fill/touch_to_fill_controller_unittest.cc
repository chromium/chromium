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
               void(const base::string16&, const base::string16&));
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

UiCredential MakeUiCredential(
    base::StringPiece username,
    base::StringPiece password,
    base::StringPiece origin = kExampleCom,
    IsPublicSuffixMatch is_public_suffix_match = IsPublicSuffixMatch(false),
    IsAffiliationBasedMatch is_affiliation_based_match =
        IsAffiliationBasedMatch(false)) {
  return UiCredential(base::UTF8ToUTF16(username), base::UTF8ToUTF16(password),
                      url::Origin::Create(GURL(origin)), is_public_suffix_match,
                      is_affiliation_based_match);
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
  base::test::TaskEnvironment task_environment_;
  MockTouchToFillView* mock_view_ = nullptr;
  MockPasswordManagerDriver driver_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  TouchToFillController touch_to_fill_controller_{
      base::PassKey<TouchToFillControllerTest>()};
};

TEST_F(TouchToFillControllerTest, Show_And_Fill) {
  UiCredential credentials[] = {MakeUiCredential("alice", "p4ssw0rd")};

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

  UiCredential credentials[] = {MakeUiCredential("alice", "p4ssw0rd")};

  EXPECT_CALL(view(),
              Show(Eq(GURL("http://example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  UiCredential credentials[] = {
      MakeUiCredential("alice", "p4ssw0rd"),
      MakeUiCredential("bob", "s3cr3t", base::StringPiece(),
                       IsPublicSuffixMatch(false),
                       IsAffiliationBasedMatch(true)),
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

TEST_F(TouchToFillControllerTest, Dismiss) {
  UiCredential credentials[] = {MakeUiCredential("alice", "p4ssw0rd")};

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
