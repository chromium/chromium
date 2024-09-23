// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_notifications_instance.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakeNotificationsInstance::FakeNotificationsInstance() = default;
FakeNotificationsInstance::~FakeNotificationsInstance() = default;

void FakeNotificationsInstance::SendNotificationEventToAndroid(
    const std::string& key,
    mojom::ArcNotificationEvent event) {
  events_.emplace_back(key, event);
}

void FakeNotificationsInstance::SendNotificationButtonClickToAndroid(
    const std::string& key,
    uint32_t button_index,
    const std::string& input) {}

void FakeNotificationsInstance::CreateNotificationWindow(
    const std::string& key) {}

void FakeNotificationsInstance::CloseNotificationWindow(
    const std::string& key) {}

void FakeNotificationsInstance::OpenNotificationSettings(
    const std::string& key) {}

void FakeNotificationsInstance::PopUpAppNotificationSettings(
    const std::string& key) {}

void FakeNotificationsInstance::OpenNotificationSnoozeSettings(
    const std::string& key) {}

void FakeNotificationsInstance::SetDoNotDisturbStatusOnAndroid(
    mojom::ArcDoNotDisturbStatusPtr status) {
  latest_do_not_disturb_status_ = std::move(status);
}

void FakeNotificationsInstance::CancelPress(const std::string& key) {}

void FakeNotificationsInstance::Init(
    mojo::PendingRemote<mojom::NotificationsHost> host_remote,
    InitCallback callback) {
  std::move(callback).Run();
}

const std::vector<std::pair<std::string, mojom::ArcNotificationEvent>>&
FakeNotificationsInstance::events() const {
  return events_;
}

const mojom::ArcDoNotDisturbStatusPtr&
FakeNotificationsInstance::latest_do_not_disturb_status() const {
  return latest_do_not_disturb_status_;
}

void FakeNotificationsInstance::PerformDeferredUserAction(uint32_t action_id) {}
void FakeNotificationsInstance::CancelDeferredUserAction(uint32_t action_id) {}
void FakeNotificationsInstance::SetLockScreenSettingOnAndroid(
    mojom::ArcLockScreenNotificationSettingPtr setting) {}
void FakeNotificationsInstance::SetNotificationConfiguration(
    mojom::NotificationConfigurationPtr configuration) {}
void FakeNotificationsInstance::OnMessageCenterVisibilityChanged(
    mojom::MessageCenterVisibility visibility) {}

}  // namespace arc
