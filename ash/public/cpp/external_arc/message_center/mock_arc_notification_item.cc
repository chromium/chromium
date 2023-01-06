// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_item.h"

#include <utility>

#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "base/functional/callback_helpers.h"

namespace ash {

MockArcNotificationItem::MockArcNotificationItem(
    const std::string& notification_key)
    : notification_key_(notification_key),
      notification_id_(kArcNotificationIdPrefix + notification_key) {}

MockArcNotificationItem::~MockArcNotificationItem() {
  for (auto& observer : observers_)
    observer.OnItemDestroying();
}

void MockArcNotificationItem::SetCloseCallback(
    base::OnceClosure close_callback) {
  close_callback_ = std::move(close_callback);
}

void MockArcNotificationItem::Close(bool by_user) {
  count_close_++;

  if (close_callback_)
    std::move(close_callback_).Run();
}

const gfx::ImageSkia& MockArcNotificationItem::GetSnapshot() const {
  return snapshot_;
}

const std::string& MockArcNotificationItem::GetNotificationKey() const {
  return notification_key_;
}

const std::string& MockArcNotificationItem::GetNotificationId() const {
  return notification_id_;
}

void MockArcNotificationItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockArcNotificationItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

arc::mojom::ArcNotificationType MockArcNotificationItem::GetNotificationType()
    const {
  return arc::mojom::ArcNotificationType::SIMPLE;
}

arc::mojom::ArcNotificationExpandState MockArcNotificationItem::GetExpandState()
    const {
  return arc::mojom::ArcNotificationExpandState::FIXED_SIZE;
}

gfx::Rect MockArcNotificationItem::GetSwipeInputRect() const {
  return gfx::Rect();
}

bool MockArcNotificationItem::IsManuallyExpandedOrCollapsed() const {
  return false;
}

}  // namespace ash
