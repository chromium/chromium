// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_
#define ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_

#include "ash/components/phonehub/notification.h"
#include "base/observer_list_types.h"

namespace ash {
namespace phonehub {

// Interface used to listen for a recent app click.
class RecentAppClickObserver : public base::CheckedObserver {
 public:
  ~RecentAppClickObserver() override = default;
  // Called when the user clicks the recent app which has an open
  // action in the PhoneHub.
  virtual void OnRecentAppClicked(
      const Notification::AppMetadata& app_metadata) = 0;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_
