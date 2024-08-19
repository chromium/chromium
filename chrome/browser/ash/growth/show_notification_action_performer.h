// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// A simple notification delegate which invokes the passed closure when the body
// or a button is clicked and notification is closed.
class HandleNotificationClickAndCloseDelegate
    : public message_center::NotificationDelegate {
 public:
  // The parameter is the index of the button that was clicked, or nullopt if
  // the body was clicked.
  using ButtonClickCallback = base::RepeatingCallback<void(std::optional<int>)>;
  using CloseCallback = base::RepeatingCallback<void(bool)>;

  // Creates a delegate that handles clicks on a button or on the body.
  explicit HandleNotificationClickAndCloseDelegate(
      const ButtonClickCallback& click_callback,
      const CloseCallback& close_callback);

  HandleNotificationClickAndCloseDelegate(
      const HandleNotificationClickAndCloseDelegate&) = delete;
  HandleNotificationClickAndCloseDelegate& operator=(
      const HandleNotificationClickAndCloseDelegate&) = delete;

  // message_center::NotificationDelegate overrides:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

 protected:
  ~HandleNotificationClickAndCloseDelegate() override;

 private:
  ButtonClickCallback click_callback_;
  CloseCallback close_callback_;
};

// Implements show system notification action for the growth framework.
class ShowNotificationActionPerformer : public UiActionPerformer {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleIdForTesting);

  ShowNotificationActionPerformer();
  ~ShowNotificationActionPerformer() override;

  // growth::Action:
  void Run(int campaign_id,
           std::optional<int> group_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;

 private:
  void HandleNotificationClicked(const base::Value::Dict* params,
                                 const std::string& notification_id,
                                 int campaign_id,
                                 std::optional<int> group_id,
                                 bool should_log_cros_events,
                                 std::optional<int> button_index);
  void HandleNotificationClose(int campaign_id,
                               std::optional<int> group_id,
                               bool should_mark_dismissed,
                               bool should_log_cros_events,
                               bool by_user);

  int current_campaign_id_;

  base::WeakPtrFactory<ShowNotificationActionPerformer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_
