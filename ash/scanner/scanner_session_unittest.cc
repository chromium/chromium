// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

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

}  // namespace
}  // namespace ash
