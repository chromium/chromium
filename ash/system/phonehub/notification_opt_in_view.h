// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace chromeos {
namespace phonehub {
class NotificationAccessManager;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for notifications from the phone.
class ASH_EXPORT NotificationOptInView
    : public views::View,
      public chromeos::phonehub::NotificationAccessManager::Observer {
 public:
  METADATA_HEADER(NotificationOptInView);

  explicit NotificationOptInView(chromeos::phonehub::NotificationAccessManager*
                                     notification_access_manager);
  NotificationOptInView(const NotificationOptInView&) = delete;
  NotificationOptInView& operator=(const NotificationOptInView&) = delete;
  ~NotificationOptInView() override;

  // chromeos::phonehub::NotificationAccessManager::Observer:
  void OnNotificationAccessChanged() override;
 private:
  void InitLayout();

  void SetUpButtonPressed();
  void DismissButtonPressed();

  // Calculates whether this view should be visible and updates its visibility
  // accordingly.
  void UpdateVisibility();

  // Main components of this view. Owned by view hierarchy.
  views::Label* text_label_ = nullptr;
  views::LabelButton* set_up_button_ = nullptr;
  views::LabelButton* dismiss_button_ = nullptr;

  chromeos::phonehub::NotificationAccessManager* notification_access_manager_;

  base::ScopedObservation<
      chromeos::phonehub::NotificationAccessManager,
      chromeos::phonehub::NotificationAccessManager::Observer>
      access_manager_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
