// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "base/scoped_observer.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
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
  InterstitialViewButton* set_up_button_ = nullptr;
  InterstitialViewButton* dismiss_button_ = nullptr;

  chromeos::phonehub::NotificationAccessManager* notification_access_manager_;

  ScopedObserver<chromeos::phonehub::NotificationAccessManager,
                 chromeos::phonehub::NotificationAccessManager::Observer>
      access_manager_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
