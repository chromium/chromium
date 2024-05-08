// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

class FakeNotificationsInstance : public mojom::NotificationsInstance {
 public:
  FakeNotificationsInstance();

  FakeNotificationsInstance(const FakeNotificationsInstance&) = delete;
  FakeNotificationsInstance& operator=(const FakeNotificationsInstance&) =
      delete;

  ~FakeNotificationsInstance() override;

  // mojom::NotificationsInstance overrides:
  void Init(mojo::PendingRemote<mojom::NotificationsHost> host_remote,
            InitCallback callback) override;

  void SendNotificationEventToAndroid(
      const std::string& key,
      mojom::ArcNotificationEvent event) override;
  void SendNotificationButtonClickToAndroid(const std::string& key,
                                            uint32_t button_index,
                                            const std::string& input) override;
  void CreateNotificationWindow(const std::string& key) override;
  void CloseNotificationWindow(const std::string& key) override;
  void OpenNotificationSettings(const std::string& key) override;
  void PopUpAppNotificationSettings(const std::string& key) override;
  void OpenNotificationSnoozeSettings(const std::string& key) override;
  void SetDoNotDisturbStatusOnAndroid(
      mojom::ArcDoNotDisturbStatusPtr status) override;
  void CancelPress(const std::string& key) override;
  void PerformDeferredUserAction(uint32_t action_id) override;
  void CancelDeferredUserAction(uint32_t action_id) override;
  void SetLockScreenSettingOnAndroid(
      mojom::ArcLockScreenNotificationSettingPtr setting) override;
  void SetNotificationConfiguration(
      mojom::NotificationConfigurationPtr configuration) override;
  void OnMessageCenterVisibilityChanged(
      mojom::MessageCenterVisibility visibility) override;

  const std::vector<std::pair<std::string, mojom::ArcNotificationEvent>>&
  events() const;
  const mojom::ArcDoNotDisturbStatusPtr& latest_do_not_disturb_status() const;

 private:
  std::vector<std::pair<std::string, mojom::ArcNotificationEvent>> events_;
  mojom::ArcDoNotDisturbStatusPtr latest_do_not_disturb_status_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
