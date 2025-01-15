// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_unpopulated_action.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "components/manta/proto/scanner.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::EqualsProto;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Property;

struct TestCase {
  manta::proto::ScannerAction unpopulated_proto;
  manta::proto::ScannerAction populated_proto;
  manta::proto::ScannerAction different_proto;
};

class ScannerUnpopulatedActionTestWithParam
    : public ::testing::TestWithParam<TestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    ScannerUnpopulatedActionTestWithParam,
    testing::Values(
        []() {
          TestCase test_case;
          manta::proto::ScannerAction action;
          manta::proto::NewEventAction& event_action =
              *action.mutable_new_event();
          test_case.unpopulated_proto = action;

          event_action.set_title("🌏");
          event_action.set_description("formerly \"Geo Sync\"");
          event_action.set_dates("20241014T160000/20241014T161500");
          event_action.set_location("Wonderland");
          test_case.populated_proto = action;

          test_case.different_proto.mutable_new_contact();

          return test_case;
        }(),
        []() {
          TestCase test_case;
          manta::proto::ScannerAction action;
          manta::proto::NewContactAction& contact_action =
              *action.mutable_new_contact();
          test_case.unpopulated_proto = action;

          contact_action.set_given_name("André");
          contact_action.set_family_name("François");
          contact_action.set_email("afrancois@example.com");
          contact_action.set_phone("+61400000000");
          test_case.populated_proto = action;

          test_case.different_proto.mutable_new_event();

          return test_case;
        }(),
        []() {
          TestCase test_case;
          manta::proto::ScannerAction action;
          manta::proto::NewGoogleDocAction& doc_action =
              *action.mutable_new_google_doc();
          test_case.unpopulated_proto = action;

          doc_action.set_title("Doc Title");
          doc_action.set_html_contents("<span>Contents</span>");
          test_case.populated_proto = action;

          test_case.different_proto.mutable_new_event();

          return test_case;
        }(),
        []() {
          TestCase test_case;
          manta::proto::ScannerAction action;
          manta::proto::NewGoogleSheetAction& sheet_action =
              *action.mutable_new_google_sheet();
          test_case.unpopulated_proto = action;

          sheet_action.set_title("Sheet Title");
          sheet_action.set_csv_contents("a,b\n1,2");
          test_case.populated_proto = action;

          test_case.different_proto.mutable_new_event();

          return test_case;
        }(),
        []() {
          TestCase test_case;
          manta::proto::ScannerAction action;
          manta::proto::CopyToClipboardAction& copy_action =
              *action.mutable_copy_to_clipboard();
          test_case.unpopulated_proto = action;

          copy_action.set_plain_text("Hello");
          copy_action.set_html_text("<b>Hello</b>");
          test_case.populated_proto = action;

          test_case.different_proto.mutable_new_event();

          return test_case;
        }()));

TEST(ScannerUnpopulatedActionTest, FromUnsetProtoReturnsNullopt) {
  manta::proto::ScannerAction unpopulated_proto;

  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(
          unpopulated_proto, /*populate_to_proto_callback=*/base::DoNothing());

  EXPECT_FALSE(unpopulated_action.has_value());
}

TEST_P(ScannerUnpopulatedActionTestWithParam, FromValidProtoReturnsValue) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;

  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(
          unpopulated_proto, /*populate_to_proto_callback=*/base::DoNothing());

  EXPECT_TRUE(unpopulated_action.has_value());
}

TEST_P(ScannerUnpopulatedActionTestWithParam, ActionCaseReturnsSameAsInput) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;

  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(
          unpopulated_proto, /*populate_to_proto_callback=*/base::DoNothing());

  EXPECT_THAT(
      unpopulated_action,
      Optional(Property("action_case", &ScannerUnpopulatedAction::action_case,
                        unpopulated_proto.action_case())));
}

TEST_P(ScannerUnpopulatedActionTestWithParam,
       PopulateCallsPopulateCallbackWithUnpopulatedProto) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;
  testing::StrictMock<
      base::MockCallback<ScannerUnpopulatedAction::PopulateCallback>>
      populate_callback;
  EXPECT_CALL(populate_callback, Run(EqualsProto(unpopulated_proto), _))
      .Times(1);
  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(unpopulated_proto,
                                          populate_callback.Get());
  ASSERT_TRUE(unpopulated_action.has_value());

  unpopulated_action->Populate(base::DoNothing());
}

TEST_P(ScannerUnpopulatedActionTestWithParam,
       PopulateRepeatedlyCallsPopulateCallbackWithUnpopulatedProto) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;
  testing::StrictMock<
      base::MockCallback<ScannerUnpopulatedAction::PopulateCallback>>
      populate_callback;
  EXPECT_CALL(populate_callback, Run(EqualsProto(unpopulated_proto), _))
      .Times(3);
  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(unpopulated_proto,
                                          populate_callback.Get());
  ASSERT_TRUE(unpopulated_action.has_value());

  unpopulated_action->Populate(base::DoNothing());
  unpopulated_action->Populate(base::DoNothing());
  unpopulated_action->Populate(base::DoNothing());
}

TEST_P(ScannerUnpopulatedActionTestWithParam, PopulateReturnsPopulated) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;
  testing::StrictMock<
      base::MockCallback<ScannerUnpopulatedAction::PopulateCallback>>
      populate_to_proto_callback;
  EXPECT_CALL(populate_to_proto_callback, Run)
      .WillOnce(RunOnceCallback<1>(GetParam().populated_proto));
  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(unpopulated_proto,
                                          populate_to_proto_callback.Get());
  ASSERT_TRUE(unpopulated_action.has_value());

  base::test::TestFuture<manta::proto::ScannerAction> future;
  unpopulated_action->Populate(future.GetCallback());

  EXPECT_THAT(future.Take(), EqualsProto(GetParam().populated_proto));
}

TEST_P(ScannerUnpopulatedActionTestWithParam,
       PopulateRepeatedlyReturnsPopulatedVariant) {
  manta::proto::ScannerAction unpopulated_proto = GetParam().unpopulated_proto;
  testing::StrictMock<
      base::MockCallback<ScannerUnpopulatedAction::PopulateCallback>>
      populate_to_proto_callback;
  EXPECT_CALL(populate_to_proto_callback, Run)
      .Times(3)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(GetParam().populated_proto));
  std::optional<ScannerUnpopulatedAction> unpopulated_action =
      ScannerUnpopulatedAction::FromProto(unpopulated_proto,
                                          populate_to_proto_callback.Get());
  ASSERT_TRUE(unpopulated_action.has_value());

  base::test::TestFuture<manta::proto::ScannerAction> future;
  unpopulated_action->Populate(future.GetCallback());
  EXPECT_THAT(future.Take(), EqualsProto(GetParam().populated_proto));

  unpopulated_action->Populate(future.GetCallback());
  EXPECT_THAT(future.Take(), EqualsProto(GetParam().populated_proto));

  unpopulated_action->Populate(future.GetCallback());
  EXPECT_THAT(future.Take(), EqualsProto(GetParam().populated_proto));
}

}  // namespace
}  // namespace ash
