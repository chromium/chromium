// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_
#define ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

namespace global_media_controls {
class MediaItemManager;
class MediaItemUIDeviceSelector;
class MediaItemUIFooter;
enum class GlobalMediaControlsEntryPoint;
}  // namespace global_media_controls

namespace media_message_center {
class MediaNotificationItem;
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

  // Returns a MediaNotificationListView that will show a list of
  // MediaItemUIView for all the active media items. If
  // `show_devices_for_item_id` is not empty, when the list shows the item for
  // this ID, it will expand the casting device list too.
  virtual std::unique_ptr<views::View> GetMediaNotificationListView(
      int separator_thickness,
      bool should_clip_height,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      const std::string& show_devices_for_item_id = "") = 0;

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

  // Use MediaNotificationProvider as a bridge to build a device selector view
  // for the given media notification item with id. `show_devices` indicates
  // whether the view should show the devices by default.
  virtual std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
  BuildDeviceSelectorView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      bool show_devices = false) = 0;

  // Use MediaNotificationProvider as a bridge to build a footer view for the
  // given media notification item.
  virtual std::unique_ptr<global_media_controls::MediaItemUIFooter>
  BuildFooterView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_NOTIFICATION_PROVIDER_H_
