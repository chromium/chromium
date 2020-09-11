// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MEDIA_NOTIFICATION_PROVIDER_H_
#define ASH_PUBLIC_CPP_MEDIA_NOTIFICATION_PROVIDER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {
class View;
}  // namespace views

namespace ash {
class MediaNotificationProviderObserver;

// Interface used to send media notification info from browser to ash.
class ASH_PUBLIC_EXPORT MediaNotificationProvider {
 public:
  // Get the global instance.
  static MediaNotificationProvider* Get();

  // Set the global instance.
  static void Set(MediaNotificationProvider* provider);

  virtual void AddObserver(MediaNotificationProviderObserver* observer) = 0;
  virtual void RemoveObserver(MediaNotificationProviderObserver* observer) = 0;

  // True if there are non-frozen media session notifications or active cast
  // notifications.
  virtual bool HasActiveNotifications() = 0;

  // True if there are active frozen media session notifications.
  virtual bool HasFrozenNotifications() = 0;

  // Returns a MediaNotificationListView populated with the correct
  // MediaNotificationContainerImpls. Used to populate the dialog on the Ash
  // shelf.
  virtual std::unique_ptr<views::View> GetMediaNotificationListView(
      SkColor separator_color,
      int separator_thickness) = 0;

  // Returns a MediaNotificationContainerimplView for the active MediaSession.
  // Displayed in the quick settings of the Ash shelf.
  virtual std::unique_ptr<views::View> GetActiveMediaNotificationView() = 0;

  // Used for ash to notify the bubble is closing.
  virtual void OnBubbleClosing() = 0;

 protected:
  virtual ~MediaNotificationProvider() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MEDIA_NOTIFICATION_PROVIDER_H_
