// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

// Handles incoming messages for the remote copy feature.
class RemoteCopyMessageHandler : public SharingMessageHandler,
                                 public ImageDecoder::ImageRequest {
 public:
  explicit RemoteCopyMessageHandler(Profile* profile);
  ~RemoteCopyMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 DoneCallback done_callback) override;

  // ImageDecoder::ImageRequest implementation:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  bool IsImageSourceAllowed(const GURL& image_url);

 private:
  void HandleText(const std::string& text);
  void HandleImage(const std::string& image_url);
  void OnImageResponseStarted(
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_head);
  void OnImageDownloadProgress(uint64_t current);
  void UpdateProgressNotification(const base::string16& context);
  void ClearProgressAndCloseNotification();
  void OnProgressNotificationAction(base::Optional<int> button, bool closed);
  void OnURLLoadComplete(std::unique_ptr<std::string> content);
  void WriteImageAndShowNotification(const SkBitmap& original_image,
                                     const SkBitmap& resized_image);
  void ShowNotification(const base::string16& title, const SkBitmap& image);
  void DetectWrite(uint64_t old_sequence_number,
                   base::TimeTicks start_ticks,
                   bool is_image);
  void Finish(RemoteCopyHandleMessageResult result);
  void CancelAsyncTasks();

  Profile* profile_ = nullptr;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::CancelableOnceCallback<void(const SkBitmap&)> resize_callback_;
  std::string device_name_;
  base::ElapsedTimer timer_;
  base::OneShotTimer write_detection_timer_;
  int64_t image_content_length_ = -1;
  int64_t image_content_progress_ = 0;
  std::string notification_id_;
  bool progress_notification_closed_ = false;
  base::OneShotTimer image_download_update_progress_timer_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCopyMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
