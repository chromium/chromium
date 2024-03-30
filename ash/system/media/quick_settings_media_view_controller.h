// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"

namespace global_media_controls {
class MediaItemManager;
class MediaSessionItemProducer;
}  // namespace global_media_controls

namespace views {
class View;
}  // namespace views

namespace ash {

class QuickSettingsMediaView;
class UnifiedSystemTrayController;

// Controller of QuickSettingsMediaView which receives and handles media item
// update events.
class ASH_EXPORT QuickSettingsMediaViewController
    : public global_media_controls::MediaDialogDelegate,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  explicit QuickSettingsMediaViewController(
      UnifiedSystemTrayController* tray_controller);
  QuickSettingsMediaViewController(const QuickSettingsMediaViewController&) =
      delete;
  QuickSettingsMediaViewController& operator=(
      const QuickSettingsMediaViewController&) = delete;
  ~QuickSettingsMediaViewController() override;

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item)
      override {}
  void HideMediaDialog() override {}
  void Focus() override {}

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override {}
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUIClicked(const std::string& id,
                            bool activate_original_media) override;
  void OnMediaItemUIShowDevices(const std::string& id) override;

  std::unique_ptr<views::View> CreateView();

  // Sets whether the quick settings view should show any media item.
  void SetShowMediaView(bool show_media_view);

  // Updates the order of media items in the quick settings media view.
  void UpdateMediaItemOrder();

  // Returns the current desired height of the media view.
  int GetMediaViewHeight();

  // Helper functions for testing.
  QuickSettingsMediaView* media_view_for_testing() { return media_view_; }

 private:
  raw_ptr<UnifiedSystemTrayController> tray_controller_ = nullptr;

  std::unique_ptr<global_media_controls::MediaItemManager> media_item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;

  global_media_controls::MediaItemUIObserverSet media_item_ui_observer_set_{
      this};

  raw_ptr<QuickSettingsMediaView, DanglingUntriaged> media_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTROLLER_H_
