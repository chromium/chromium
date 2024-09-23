// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_IMPL_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/account_id/account_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/message_center/message_center.h"

namespace ash {

// The class represents each ARC notification. One instance of this class
// corresponds to one ARC notification.
class ArcNotificationItemImpl : public ArcNotificationItem {
 public:
  ArcNotificationItemImpl(ArcNotificationManager* manager,
                          message_center::MessageCenter* message_center,
                          const std::string& notification_key,
                          const AccountId& profile_id);

  ArcNotificationItemImpl(const ArcNotificationItemImpl&) = delete;
  ArcNotificationItemImpl& operator=(const ArcNotificationItemImpl&) = delete;

  ~ArcNotificationItemImpl() override;

  // ArcNotificationItem overrides:
  void OnClosedFromAndroid() override;
  void OnUpdatedFromAndroid(arc::mojom::ArcNotificationDataPtr data,
                            const std::string& app_id) override;
  void Close(bool by_user) override;
  void Click() override;
  void ClickButton(const int button_index, const std::string& input) override;
  void OpenSettings() override;
  void DisableNotification() override;
  void OpenSnooze() override;
  void ToggleExpansion() override;
  void SetExpandState(bool expanded) override;
  void OnWindowActivated(bool activated) override;
  void OnRemoteInputActivationChanged(bool activated) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void IncrementWindowRefCount() override;
  void DecrementWindowRefCount() override;
  const gfx::ImageSkia& GetSnapshot() const override;
  arc::mojom::ArcNotificationType GetNotificationType() const override;
  arc::mojom::ArcNotificationExpandState GetExpandState() const override;
  bool IsManuallyExpandedOrCollapsed() const override;
  gfx::Rect GetSwipeInputRect() const override;
  const std::string& GetNotificationKey() const override;
  const std::string& GetNotificationId() const override;
  void CancelPress() override;

 private:
  const raw_ptr<ArcNotificationManager> manager_;
  const raw_ptr<message_center::MessageCenter> message_center_;

  // The snapshot of the latest notification.
  gfx::ImageSkia snapshot_;
  // The type of the latest notification.
  arc::mojom::ArcNotificationType type_ =
      arc::mojom::ArcNotificationType::SIMPLE;
  // The expand state of the latest notification.
  arc::mojom::ArcNotificationExpandState expand_state_ =
      arc::mojom::ArcNotificationExpandState::FIXED_SIZE;
  // The type of shown content of the latest notification.
  arc::mojom::ArcNotificationShownContents shown_contents_ =
      arc::mojom::ArcNotificationShownContents::CONTENTS_SHOWN;
  // Rect indicating where Android wants to handle swipe events by itself.
  gfx::Rect swipe_input_rect_ = gfx::Rect();
  // The reference counter of the window.
  int window_ref_count_ = 0;

  base::ObserverList<Observer>::Unchecked observers_;

  const AccountId profile_id_;
  const std::string notification_key_;
  const std::string notification_id_;

  // The flag to indicate that the removing is initiated by the manager and we
  // don't need to notify a remove event to the manager.
  // This is true only when:
  //   (1) the notification is being removed
  //   (2) the removing is initiated by manager
  bool being_removed_by_manager_ = false;

  bool manually_expanded_or_collapsed_ = false;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ArcNotificationItemImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_IMPL_H_
