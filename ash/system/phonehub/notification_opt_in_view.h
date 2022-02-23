// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/components/phonehub/notification_access_manager.h"
#include "ash/system/phonehub/sub_feature_opt_in_view.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// An additional entry point shown on the Phone Hub bubble for the user to grant
// access or opt out for notifications from the phone.
class ASH_EXPORT NotificationOptInView
    : public SubFeatureOptInView,
      public phonehub::NotificationAccessManager::Observer {
 public:
  METADATA_HEADER(NotificationOptInView);

  explicit NotificationOptInView(
      phonehub::NotificationAccessManager* notification_access_manager);
  NotificationOptInView(const NotificationOptInView&) = delete;
  NotificationOptInView& operator=(const NotificationOptInView&) = delete;
  ~NotificationOptInView() override;

  // phonehub::NotificationAccessManager::Observer:
  void OnNotificationAccessChanged() override;
 private:
  void SetUpButtonPressed() override;
  void DismissButtonPressed() override;

  // Calculates whether this view should be visible and updates its visibility
  // accordingly.
  void UpdateVisibility();

  phonehub::NotificationAccessManager* notification_access_manager_;

  base::ScopedObservation<phonehub::NotificationAccessManager,
                          phonehub::NotificationAccessManager::Observer>
      access_manager_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_NOTIFICATION_OPT_IN_VIEW_H_
