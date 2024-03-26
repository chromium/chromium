// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/browser.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/native_theme/native_theme.h"

class SkBitmap;

namespace test {
class DownloadItemNotificationTest;
}

namespace message_center {
class Notification;
}

// Handles the notification on ChromeOS for one download item.
class DownloadItemNotification : public ImageDecoder::ImageRequest,
                                 public message_center::NotificationObserver,
                                 public DownloadUIModel::Delegate {
 public:
  DownloadItemNotification(Profile* profile,
                           DownloadUIModel::DownloadUIModelPtr item);

  DownloadItemNotification(const DownloadItemNotification&) = delete;
  DownloadItemNotification& operator=(const DownloadItemNotification&) = delete;

  ~DownloadItemNotification() override;

  // Observer for this notification.
  class Observer {
   public:
    virtual void OnDownloadDestroyed(const ContentId& contentId) {}
  };

  // Set an observer for this notification.
  void SetObserver(Observer* observer);

  DownloadUIModel* GetDownload();

  // DownloadUIModel::Delegate overrides.
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed(const ContentId& id) override;

  // Disables popup by setting low priority.
  void DisablePopup();

  // NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  friend class test::DownloadItemNotificationTest;

  enum ImageDecodeStatus { NOT_STARTED, IN_PROGRESS, DONE, FAILED, NOT_IMAGE };

  enum NotificationUpdateType { ADD, UPDATE, UPDATE_AND_POPUP };

  std::string GetNotificationId() const;

  void CloseNotification();
  void Update();
  void UpdateNotificationData(bool display, bool force_pop_up);
  SkColor GetNotificationIconColor();

  // Set preview image of the notification. Must be called on IO thread.
  void OnImageLoaded(std::string image_data);
  void OnImageCropped(const SkBitmap& image);

  // ImageDecoder::ImageRequest overrides:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Returns a short one-line status string for the download.
  std::u16string GetTitle() const;

  // Returns a short one-line status string for a download command.
  std::u16string GetCommandLabel(DownloadCommands::Command command) const;

  // Get the warning text to notify a dangerous download. Should only be called
  // if IsDangerous() is true.
  std::u16string GetWarningStatusString() const;

  // Get the sub status text of the current in-progress download status. Should
  // be called only for downloads in progress.
  std::u16string GetInProgressSubStatusString() const;

  // Get the sub status text. Can be called for downloads in all states.
  // If the state does not have sub status string, it returns empty string.
  std::u16string GetSubStatusString() const;

  // Get the status text.
  std::u16string GetStatusString() const;

  bool IsScanning() const;
  bool AllowedToOpenWhileScanning() const;

  Browser* GetBrowser() const;
  Profile* profile() const;

  // Returns the list of possible extra (all except the default) actions.
  std::unique_ptr<std::vector<DownloadCommands::Command>> GetExtraActions()
      const;

  // The profile associated with this notification.
  raw_ptr<Profile> profile_;

  // Observer of this notification.
  raw_ptr<Observer> observer_;

  // Flag to show the notification on next update. If true, the notification
  // goes visible. The initial value is true so it gets shown on initial update.
  bool show_next_ = true;

  // Flag if the notification has been closed or not. Setting this flag
  // prevents updates after close.
  bool closed_ = false;

  // Flag to indicate that a review dialog is open for the user to accept or
  // bypass an enterprise warning on the download. If this is true, the "Review"
  // button should be removed from the notification.
  bool in_review_ = false;

  download::DownloadItem::DownloadState previous_download_state_ =
      download::DownloadItem::MAX_DOWNLOAD_STATE;  // As uninitialized state
  bool previous_dangerous_state_ = false;
  bool previous_insecure_state_ = false;
  std::unique_ptr<message_center::Notification> notification_;

  DownloadUIModel::DownloadUIModelPtr item_;
  std::unique_ptr<std::vector<DownloadCommands::Command>> button_actions_;

  // Status of the preview image decode.
  ImageDecodeStatus image_decode_status_ = NOT_STARTED;

  base::WeakPtrFactory<DownloadItemNotification> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_
