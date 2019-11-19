// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller_impl.h"

#include <memory>

#include "ash/media/media_notification_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/unguessable_token.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

media_session::mojom::MediaSessionInfoPtr BuildMediaSessionInfo(
    bool is_controllable) {
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->is_controllable = is_controllable;
  return session_info;
}

media_session::mojom::AudioFocusRequestStatePtr GetRequestStateWithId(
    const base::UnguessableToken& id) {
  media_session::mojom::AudioFocusRequestStatePtr session(
      media_session::mojom::AudioFocusRequestState::New());
  session->request_id = id;
  session->session_info = BuildMediaSessionInfo(true);
  return session;
}

}  // namespace

class MediaNotificationControllerImplTest : public AshTestBase {
 public:
  MediaNotificationControllerImplTest()
      : task_runner_(new base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kStandalone)) {}

  ~MediaNotificationControllerImplTest() override = default;

  // AshTestBase
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaSessionNotification);

    AshTestBase::SetUp();

    Shell::Get()->media_notification_controller()->set_task_runner_for_testing(
        task_runner_);
  }

  void ExpectNotificationCount(unsigned count) {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();

    EXPECT_EQ(count, message_center->GetVisibleNotifications().size());

    // Media notifications should never be shown as a popup so we always check
    // this is empty.
    EXPECT_TRUE(message_center->GetPopupNotifications().empty());
  }

  media_session::MediaMetadata BuildMediaMetadata() {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    return metadata;
  }

  void ExpectHistogramCountRecorded(int count, int size) {
    histogram_tester_.ExpectBucketCount(
        media_message_center::kCountHistogramName, count, size);
  }

  void ExpectHistogramSourceRecorded(
      media_message_center::MediaNotificationItem::Source source) {
    histogram_tester_.ExpectUniqueSample(
        media_message_center::MediaNotificationItem::kSourceHistogramName,
        static_cast<base::HistogramBase::Sample>(source), 1);
  }

  void SimulateSessionLock(bool locked) {
    SessionInfo info;
    info.state = locked ? session_manager::SessionState::LOCKED
                        : session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  void SimulateFreezeTimerExpired() {
    task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(2500));
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationControllerImplTest);
};

// Test toggling the notification multiple times with the same ID. Since the
// notification is keyed by ID we should only ever show one.
TEST_F(MediaNotificationControllerImplTest, OnFocusGainedLost_SameId) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id));
  SimulateFreezeTimerExpired();

  ExpectNotificationCount(0);
}

// Test toggling the notification multiple times with different IDs. This should
// show one notification per ID.
TEST_F(MediaNotificationControllerImplTest, OnFocusGainedLost_MultipleIds) {
  base::UnguessableToken id1 = base::UnguessableToken::Create();
  base::UnguessableToken id2 = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id1));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id1.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id2));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id2.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(2);
  ExpectHistogramCountRecorded(2, 1);

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id1));
  SimulateFreezeTimerExpired();

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);
}

// Test that a notification is hidden when it becomes uncontrollable. We still
// keep the media_message_center::MediaNotificationItem around in case it
// becomes controllable again.
TEST_F(MediaNotificationControllerImplTest,
       OnFocusGained_ControllableBecomesUncontrollable) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionInfoChanged(BuildMediaSessionInfo(false));

  ExpectNotificationCount(0);
}

// Test that a notification is shown when it becomes controllable.
TEST_F(MediaNotificationControllerImplTest,
       OnFocusGained_NotControllableBecomesControllable) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  media_session::mojom::AudioFocusRequestStatePtr state =
      GetRequestStateWithId(id);
  state->session_info->is_controllable = false;
  Shell::Get()->media_notification_controller()->OnFocusGained(
      std::move(state));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(0);

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionInfoChanged(BuildMediaSessionInfo(true));

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);
}

// Test hiding a notification with an invalid ID.
TEST_F(MediaNotificationControllerImplTest, OnFocusLost_Noop) {
  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(base::UnguessableToken::Create()));

  ExpectNotificationCount(0);
}

// Test that media notifications have the correct custom view type.
TEST_F(MediaNotificationControllerImplTest, NotificationHasCustomViewType) {
  ExpectNotificationCount(0);

  base::UnguessableToken id = base::UnguessableToken::Create();

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          id.ToString());
  EXPECT_TRUE(notification);

  EXPECT_EQ(kMediaSessionNotificationCustomViewType,
            notification->custom_view_type());
}

// Test that if we recieve a null media session info that we hide the
// notification.
TEST_F(MediaNotificationControllerImplTest, HandleNullMediaSessionInfo) {
  ExpectNotificationCount(0);

  base::UnguessableToken id = base::UnguessableToken::Create();

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionInfoChanged(nullptr);

  ExpectNotificationCount(0);
}

TEST_F(MediaNotificationControllerImplTest, MediaMetadata_NoTitle) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  media_session::MediaMetadata metadata;
  metadata.artist = base::ASCIIToUTF16("artist");

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(metadata);

  ExpectNotificationCount(0);
}

TEST_F(MediaNotificationControllerImplTest, MediaMetadataUpdated_MissingInfo) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramCountRecorded(1, 1);

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(media_session::MediaMetadata());

  ExpectNotificationCount(0);
}

TEST_F(MediaNotificationControllerImplTest, RecordHistogramSource_Unknown) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramSourceRecorded(
      media_message_center::MediaNotificationItem::Source::kUnknown);
}

TEST_F(MediaNotificationControllerImplTest, RecordHistogramSource_Web) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  media_session::mojom::AudioFocusRequestStatePtr request =
      GetRequestStateWithId(id);
  request->source_name = "web";

  Shell::Get()->media_notification_controller()->OnFocusGained(
      std::move(request));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramSourceRecorded(
      media_message_center::MediaNotificationItem::Source::kWeb);
}

TEST_F(MediaNotificationControllerImplTest, RecordHistogramSource_Assistant) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  media_session::mojom::AudioFocusRequestStatePtr request =
      GetRequestStateWithId(id);
  request->source_name = "assistant";

  Shell::Get()->media_notification_controller()->OnFocusGained(
      std::move(request));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramSourceRecorded(
      media_message_center::MediaNotificationItem::Source::kAssistant);
}

TEST_F(MediaNotificationControllerImplTest, RecordHistogramSource_Arc) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  media_session::mojom::AudioFocusRequestStatePtr request =
      GetRequestStateWithId(id);
  request->source_name = "arc";

  Shell::Get()->media_notification_controller()->OnFocusGained(
      std::move(request));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  ExpectHistogramSourceRecorded(
      media_message_center::MediaNotificationItem::Source::kArc);
}

// Test that locking the screen will hide the media notifications. Unlocking the
// screen should re-show the notifications.
TEST_F(MediaNotificationControllerImplTest, HideWhenScreenLocked) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);

  // Show a non-media notification that should still be displayed.
  message_center->AddNotification(
      ash::CreateSystemNotification("test", base::string16(), base::string16(),
                                    "test", base::BindRepeating([]() {})));

  EXPECT_EQ(2u, message_center->GetVisibleNotifications().size());

  // Lock the screen and only the non-media notification should be visible.
  SimulateSessionLock(true);

  {
    auto notifications = message_center->GetVisibleNotifications();
    EXPECT_EQ(1u, notifications.size());
    EXPECT_EQ("test", (*notifications.begin())->id());
  }

  // Unlock the screen and both notifications should be visible again.
  SimulateSessionLock(false);
  EXPECT_EQ(2u, message_center->GetVisibleNotifications().size());
}

// Test that when we lose focus we freeze the notification until the timer
// is fired and then remove it.
TEST_F(MediaNotificationControllerImplTest, OnFocusLostFreezeUntilTimerFired) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id.ToString())
                   ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id));

  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  SimulateFreezeTimerExpired();
  ExpectNotificationCount(0);
}

// Test that when we lose focus we freeze the notification and then we see
// the session resume we keep the notification and unfreeze it.
TEST_F(MediaNotificationControllerImplTest, OnFocusLostFreezeAndResumeSameId) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id.ToString())
                   ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id));

  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  // The session comes back and is controllable so we should unfreeze the
  // notification.
  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id.ToString())
                   ->frozen());
}

// Test that when we lose focus we freeze the notification and then we see
// the session resume but it is missing metadata we hide the notification.
TEST_F(MediaNotificationControllerImplTest,
       OnFocusLostFreezeAndResumeSameId_MissingMetadata) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id.ToString())
                   ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id));

  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(media_session::MediaMetadata());

  // The session has come back but the metadata is missing data so we should
  // keep the notification frozen.
  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  // After the timer has been fired we should hide the notification but still
  // have the controller.
  SimulateFreezeTimerExpired();
  ExpectNotificationCount(0);
  EXPECT_TRUE(Shell::Get()->media_notification_controller()->HasItemForTesting(
      id.ToString()));
}

// Test that when we lose focus we freeze the notification and then we see
// the session resume but it is not controllable we hide the notification.
TEST_F(MediaNotificationControllerImplTest,
       OnFocusLostFreezeAndResumeSameId_NotControllable) {
  base::UnguessableToken id = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id.ToString())
                   ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id));

  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  media_session::mojom::AudioFocusRequestStatePtr state =
      GetRequestStateWithId(id);
  state->session_info->is_controllable = false;
  Shell::Get()->media_notification_controller()->OnFocusGained(
      std::move(state));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  // The session has come back but the metadata is not controllable so we should
  // keep the notification frozen.
  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id.ToString())
                  ->frozen());

  // After the timer has been fired we should hide the notification but still
  // have the controller.
  SimulateFreezeTimerExpired();
  ExpectNotificationCount(0);
  EXPECT_TRUE(Shell::Get()->media_notification_controller()->HasItemForTesting(
      id.ToString()));
}

// Test that when we lose focus we freeze the notification and we see a new
// session then that does not unfreeze the first notification.
TEST_F(MediaNotificationControllerImplTest,
       OnFocusLostFreezeAndDoNotResumeNewId) {
  base::UnguessableToken id1 = base::UnguessableToken::Create();
  base::UnguessableToken id2 = base::UnguessableToken::Create();

  ExpectNotificationCount(0);

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id1));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id1.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(1);
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id1.ToString())
                   ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      GetRequestStateWithId(id1));

  ExpectNotificationCount(1);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id1.ToString())
                  ->frozen());

  Shell::Get()->media_notification_controller()->OnFocusGained(
      GetRequestStateWithId(id2));

  Shell::Get()
      ->media_notification_controller()
      ->GetItem(id2.ToString())
      ->MediaSessionMetadataChanged(BuildMediaMetadata());

  ExpectNotificationCount(2);
  EXPECT_TRUE(Shell::Get()
                  ->media_notification_controller()
                  ->GetItem(id1.ToString())
                  ->frozen());
  EXPECT_FALSE(Shell::Get()
                   ->media_notification_controller()
                   ->GetItem(id2.ToString())
                   ->frozen());

  SimulateFreezeTimerExpired();
  ExpectNotificationCount(1);

  EXPECT_FALSE(Shell::Get()->media_notification_controller()->HasItemForTesting(
      id1.ToString()));
}

}  // namespace ash
