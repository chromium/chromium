// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_
#define ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

namespace media_message_center {
struct NotificationTheme;
}  // namespace media_message_center

namespace views {
class View;
}  // namespace views

namespace ash {

class MediaNotificationProviderObserver;

// Interface used to send media notification info from browser to ash.
class ASH_EXPORT MediaNotificationProvider {
 public:
  virtual ~MediaNotificationProvider() = default;

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
  // shelf. If `item_id` is non-empty, then the list consists only of the item
  // specified by the ID.
  virtual std::unique_ptr<views::View> GetMediaNotificationListView(
      int separator_thickness,
      bool should_clip_height,
      const std::string& item_id = "") = 0;

  // Used for ash to notify the bubble is closing.
  virtual void OnBubbleClosing() = 0;

  // Set the color theme of media notification view.
  virtual void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) = 0;

  virtual global_media_controls::MediaItemManager* GetMediaItemManager() = 0;

  // Performs initialization that must be done after the user session is
  // initialized.
  virtual void OnPrimaryUserSessionStarted() {}

  // Use MediaNotificationProvider as a bridge to add/remove a given
  // MediaItemManager to/from CastMediaNotificationProducerKeyedService, since
  // the service lives on chrome/browser/ui/ash.
  virtual void AddMediaItemManagerToCastService(
      global_media_controls::MediaItemManager* media_item_manager) {}
  virtual void RemoveMediaItemManagerFromCastService(
      global_media_controls::MediaItemManager* media_item_manager) {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_
