// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_ITEM_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_ITEM_H_

#include <string>

#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"

namespace ash {

class MockArcNotificationItem : public ArcNotificationItem {
 public:
  explicit MockArcNotificationItem(const std::string& notification_key);

  MockArcNotificationItem(const MockArcNotificationItem&) = delete;
  MockArcNotificationItem& operator=(const MockArcNotificationItem&) = delete;

  ~MockArcNotificationItem() override;

  // Methods for testing.
  size_t count_close() { return count_close_; }
  base::WeakPtr<MockArcNotificationItem> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetCloseCallback(base::OnceClosure close_callback);

  // Overriding methods for testing.
  void Close(bool by_user) override;
  const gfx::ImageSkia& GetSnapshot() const override;
  const std::string& GetNotificationKey() const override;
  const std::string& GetNotificationId() const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Overriding methods for returning dummy data or doing nothing.
  void OnClosedFromAndroid() override {}
  void Click() override {}
  void ClickButton(const int button_index, const std::string& input) override {}
  void ToggleExpansion() override {}
  void SetExpandState(bool expanded) override {}
  void OnWindowActivated(bool activated) override {}
  void OpenSettings() override {}
  void DisableNotification() override {}
  void OpenSnooze() override {}
  void IncrementWindowRefCount() override {}
  void DecrementWindowRefCount() override {}
  void OnRemoteInputActivationChanged(bool activate) override {}
  void CancelPress() override {}

  arc::mojom::ArcNotificationType GetNotificationType() const override;
  arc::mojom::ArcNotificationExpandState GetExpandState() const override;
  gfx::Rect GetSwipeInputRect() const override;

  void OnUpdatedFromAndroid(arc::mojom::ArcNotificationDataPtr data,
                            const std::string& app_id) override {}
  bool IsManuallyExpandedOrCollapsed() const override;

 private:
  std::string notification_key_;
  std::string notification_id_;
  gfx::ImageSkia snapshot_;
  size_t count_close_ = 0;

  base::ObserverList<Observer>::Unchecked observers_;
  base::OnceClosure close_callback_;

  base::WeakPtrFactory<MockArcNotificationItem> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_ITEM_H_
