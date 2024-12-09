// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/auto_reset.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithArg;

FakeScannerProfileScopedDelegate* GetFakeScannerProfileScopedDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller.delegate_for_testing()->GetProfileScopedDelegate());
}

class ScannerControllerTest : public AshTestBase {
 public:
  ScannerControllerTest() = default;
  ScannerControllerTest(const ScannerControllerTest&) = delete;
  ScannerControllerTest& operator=(const ScannerControllerTest&) = delete;
  ~ScannerControllerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kScannerUpdate};
  base::AutoReset<bool> ignore_scanner_update_secret_key_ =
      switches::SetIgnoreScannerUpdateSecretKeyForTest();
};

TEST_F(ScannerControllerTest, CanStartSessionIfSystemStateEnabled) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          GetSystemState)
      .WillByDefault(Return(
          ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{})));

  EXPECT_TRUE(scanner_controller->CanStartSession());
  EXPECT_TRUE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, CanNotStartSessionIfSystemStateBlocked) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          GetSystemState)
      .WillByDefault(Return(
          ScannerSystemState(ScannerStatus::kBlocked, /*failed_checks=*/{})));

  EXPECT_FALSE(scanner_controller->CanStartSession());
  EXPECT_FALSE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, FetchesActionsDuringActiveSession) {
  base::test::TestFuture<std::vector<ScannerActionViewModel>> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event title");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));

  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());

  EXPECT_THAT(actions_future.Take(), SizeIs(1));
}

TEST_F(ScannerControllerTest, NoActionsFetchedWhenNoActiveSession) {
  base::test::TestFuture<std::vector<ScannerActionViewModel>> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);

  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());

  EXPECT_THAT(actions_future.Take(), IsEmpty());
}

TEST_F(ScannerControllerTest, ResetsScannerSessionWhenActiveUserChanges) {
  SimulateUserLogin("user1@gmail.com");
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  EXPECT_TRUE(scanner_controller->HasActiveSessionForTesting());

  // Switch to a different user.
  SimulateUserLogin("user2@gmail.com");

  EXPECT_FALSE(scanner_controller->HasActiveSessionForTesting());
}

TEST_F(ScannerControllerTest, ShowsNotificationWhileExecutingAction) {
  base::test::TestFuture<std::vector<ScannerActionViewModel>> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event 1");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  std::vector<ScannerActionViewModel> actions = actions_future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  actions[0].ExecuteAction(/*action_finished_callback=*/base::DoNothing());

  // Notification should be shown while action is executing.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              SizeIs(1));

  // Finish executing the action.
  fetch_action_details_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(output),
      manta::MantaStatus());

  // Notification should be hidden.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              IsEmpty());
}

}  // namespace

}  // namespace ash
