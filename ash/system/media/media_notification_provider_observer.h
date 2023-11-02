// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_OBSERVER_H_
#define ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

// Observer for ash to be notified when notification info changed.
class MediaNotificationProviderObserver : public base::CheckedObserver {
 public:
  // Called when the list of notifications has chagned.
  virtual void OnNotificationListChanged() = 0;

  // Called when the size of the view representing the list of notifications
  // has changed.
  virtual void OnNotificationListViewSizeChanged() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_OBSERVER_H_
