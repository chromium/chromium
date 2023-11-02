// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_notification_provider_impl.h"

#include "ash/system/media/media_notification_provider_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/unguessable_token.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace ash {

namespace {

class MockMediaNotificationProviderObserver
    : public MediaNotificationProviderObserver {
 public:
  MOCK_METHOD0(OnNotificationListChanged, void());
  MOCK_METHOD0(OnNotificationListViewSizeChanged, void());
};

class FakeMediaSessionService : public media_session::MediaSessionService {
 public:
  FakeMediaSessionService() = default;
  FakeMediaSessionService(const FakeMediaSessionService&) = delete;
  FakeMediaSessionService& operator=(const FakeMediaSessionService&) = delete;
  ~FakeMediaSessionService() override = default;

  // media_session::MediaSessionService:
  void BindAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override {}
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override {}
  void BindMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override {}
};

class MediaSessionShellDelegate : public TestShellDelegate {
 public:
  MediaSessionShellDelegate() = default;
  ~MediaSessionShellDelegate() override = default;

  // ShellDelegate:
  media_session::MediaSessionService* GetMediaSessionService() override {
    return &media_session_service_;
  }

 private:
  FakeMediaSessionService media_session_service_;
};

}  // namespace

class MediaNotificationProviderImplTest : public AshTestBase {
 protected:
  MediaNotificationProviderImplTest() {}
  ~MediaNotificationProviderImplTest() override {}

  void SetUp() override {
    AshTestBase::SetUp(std::make_unique<MediaSessionShellDelegate>());

    provider_ = static_cast<MediaNotificationProviderImpl*>(
        MediaNotificationProvider::Get());
    mock_observer_ = std::make_unique<MockMediaNotificationProviderObserver>();
    provider_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    mock_observer_.reset();
    AshTestBase::TearDown();
  }

  void SimulateShowNotification(base::UnguessableToken id) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);

    provider_->media_session_item_producer_for_testing()->OnFocusGained(
        std::move(focus));
    provider_->media_session_item_producer_for_testing()->ActivateItem(
        id.ToString());
  }

  void SimulateHideNotification(base::UnguessableToken id) {
    provider_->media_session_item_producer_for_testing()->HideItem(
        id.ToString());
  }

  MockMediaNotificationProviderObserver* observer() {
    return mock_observer_.get();
  }

  MediaNotificationProviderImpl* provider() { return provider_; }

 private:
  views::LayoutProvider layout_provider_;

  std::unique_ptr<MockMediaNotificationProviderObserver> mock_observer_;

  MediaNotificationProviderImpl* provider_;
};

TEST_F(MediaNotificationProviderImplTest, NotificationListTest) {
  auto id_1 = base::UnguessableToken::Create();
  auto id_2 = base::UnguessableToken::Create();

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged).Times(2);
  SimulateShowNotification(id_1);
  SimulateShowNotification(id_2);
  provider()->SetColorTheme(media_message_center::NotificationTheme());
  std::unique_ptr<views::View> view =
      provider()->GetMediaNotificationListView(1);

  auto* notification_list_view =
      static_cast<global_media_controls::MediaItemUIListView*>(view.get());
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 2u);

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged);
  SimulateHideNotification(id_1);
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 1u);

  provider()->OnBubbleClosing();
}

TEST_F(MediaNotificationProviderImplTest, NotifyObserverOnListChangeTest) {
  auto id = base::UnguessableToken::Create();

  // Expecting 2 calls: one when MediaSessionNotificationItem is created in
  // MediaNotificationService::OnFocusgained, one when
  // MediaNotificationService::ShowNotification is called in
  // SimulateShownotification.
  EXPECT_CALL(*observer(), OnNotificationListChanged).Times(2);
  SimulateShowNotification(id);

  EXPECT_CALL(*observer(), OnNotificationListChanged);
  SimulateHideNotification(id);
}

// Regression test for https://crbug.com/1312419. This should not crash on ASan
// builds (or any other build of course).
TEST_F(MediaNotificationProviderImplTest, DontUseDeletedListView) {
  // Simulate a media session item.
  auto id = base::UnguessableToken::Create();
  SimulateShowNotification(id);
  provider()->SetColorTheme(media_message_center::NotificationTheme());

  // Create a list view with that item.
  std::unique_ptr<views::View> view =
      provider()->GetMediaNotificationListView(1);

  // Delete the list view.
  view.reset();

  // Hide the item. This should not call into the deleted view.
  SimulateHideNotification(id);
}

}  // namespace ash
