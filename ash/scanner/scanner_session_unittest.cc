// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

TEST(ScannerSessionTest, FetchActionsForImageReturnsEmptyWhenDelegateErrors) {
  FakeScannerProfileScopedDelegate delegate;
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  delegate.SendFakeActionsResponse(
      nullptr,
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kInvalidInput});

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsEmptyWhenDelegateHasNoObjects) {
  FakeScannerProfileScopedDelegate delegate;
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  delegate.SendFakeActionsResponse(
      std::make_unique<manta::proto::ScannerOutput>(),
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk});

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsEqualNumberOfActionsAsProtoResponse) {
  FakeScannerProfileScopedDelegate delegate;
  ScannerSession session(&delegate);
  manta::proto::NewEventAction event_action;
  event_action.set_title("Event");

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *output->add_objects();
  *object.add_actions()->mutable_new_event() = event_action;
  *object.add_actions()->mutable_new_event() = event_action;
  *object.add_actions()->mutable_new_event() = event_action;
  delegate.SendFakeActionsResponse(
      std::move(output),
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk});

  EXPECT_THAT(future.Take(), SizeIs(3));
}

TEST(ScannerSessionTest, RunningNewEventActionOpensUrl) {
  MockNewWindowDelegate new_window_delegate;
  EXPECT_CALL(new_window_delegate,
              OpenUrl(Property("spec", &GURL::spec,
                               "https://calendar.google.com/calendar/render"
                               "?action=TEMPLATE"
                               "&text=%F0%9F%8C%8F"
                               "&details=formerly+%22Geo+Sync%22"
                               "&dates=20241014T160000%2F20241014T161500"
                               "&location=Wonderland"),
                      _, _))
      .Times(1);
  FakeScannerProfileScopedDelegate delegate;
  ScannerSession session(&delegate);
  manta::proto::NewEventAction event_action;
  event_action.set_title("🌏");
  event_action.set_description("formerly \"Geo Sync\"");
  event_action.set_dates("20241014T160000/20241014T161500");
  event_action.set_location("Wonderland");

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  *output->add_objects()->add_actions()->mutable_new_event() =
      std::move(event_action);
  delegate.SendFakeActionsResponse(
      std::move(output),
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk});
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  std::move(actions.front())
      .ToCallback(action_finished_future.GetCallback())
      .Run();

  EXPECT_TRUE(action_finished_future.Get());
}

TEST(ScannerSessionTest, RunningNewContactActionOpensUrl) {
  MockNewWindowDelegate new_window_delegate;
  EXPECT_CALL(new_window_delegate,
              OpenUrl(Property("spec", &GURL::spec,
                               "https://contacts.google.com/new"
                               "?givenname=Andr%C3%A9"
                               "&familyname=Fran%C3%A7ois"
                               "&email=afrancois%40example.com"
                               "&phone=%2B61400000000"),
                      _, _))
      .Times(1);
  FakeScannerProfileScopedDelegate delegate;
  ScannerSession session(&delegate);
  manta::proto::NewContactAction contact_action;
  contact_action.set_given_name("André");
  contact_action.set_family_name("François");
  contact_action.set_email("afrancois@example.com");
  contact_action.set_phone("+61400000000");

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  *output->add_objects()->add_actions()->mutable_new_contact() =
      std::move(contact_action);
  delegate.SendFakeActionsResponse(
      std::move(output),
      manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk});
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  std::move(actions.front())
      .ToCallback(action_finished_future.GetCallback())
      .Run();

  EXPECT_TRUE(action_finished_future.Get());
}

}  // namespace
}  // namespace ash
