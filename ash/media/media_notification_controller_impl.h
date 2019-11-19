// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_IMPL_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

namespace {
class MediaNotificationBlocker;
}  // namespace

class MediaNotificationContainerImpl;

// MediaNotificationControllerImpl will show/hide media notifications when media
// sessions are active. These notifications will show metadata and playback
// controls.
class ASH_EXPORT MediaNotificationControllerImpl
    : public media_session::mojom::AudioFocusObserver,
      public media_message_center::MediaNotificationController {
 public:
  explicit MediaNotificationControllerImpl(
      service_manager::Connector* connector);
  ~MediaNotificationControllerImpl() override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_message_center::MediaNotificationController:
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
  void LogMediaSessionActionButtonPressed(const std::string& id) override {}

  std::unique_ptr<MediaNotificationContainerImpl> CreateMediaNotification(
      const message_center::Notification& notification);

  media_message_center::MediaSessionNotificationItem* GetItem(
      const std::string& id) {
    auto it = notifications_.find(id);
    DCHECK(it != notifications_.end());
    return &it->second;
  }

  bool HasItemForTesting(const std::string& id) const;
  void set_task_runner_for_testing(
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing) {
    task_runner_for_testing_ = task_runner_for_testing;
  }

 private:
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;

  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  // Stores a |media_message_center::MediaSessionNotificationItem| for each
  // media session keyed by its |request_id| in string format.
  std::map<const std::string,
           media_message_center::MediaSessionNotificationItem>
      notifications_;

  // Tick clock used for testing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing_;

  std::unique_ptr<MediaNotificationBlocker> blocker_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationControllerImpl);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_IMPL_H_
