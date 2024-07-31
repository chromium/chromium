// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "components/sharing_message/shared_clipboard/remote_copy_handle_message_result.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "ui/base/clipboard/clipboard.h"
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
  RemoteCopyMessageHandler(const RemoteCopyMessageHandler&) = delete;
  RemoteCopyMessageHandler& operator=(const RemoteCopyMessageHandler&) = delete;
  ~RemoteCopyMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

  // ImageDecoder::ImageRequest implementation:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  bool IsImageSourceAllowed(const GURL& image_url);

 private:
  friend class RemoteCopyBrowserTest;
  friend class RemoteCopyMessageHandlerTest;

  void HandleText(const std::string& text);
  void HandleImage(const std::string& image_url);
  void OnURLLoadComplete(std::unique_ptr<std::string> content);
  void WriteImageAndShowNotification(const SkBitmap& image);
  void ShowNotification(const std::u16string& title, const SkBitmap& image);
  void DetectWrite(const ui::ClipboardSequenceNumberToken& old_sequence_number,
                   base::TimeTicks start_ticks,
                   bool is_image);
  void Finish(RemoteCopyHandleMessageResult result);
  void CancelAsyncTasks();

  void set_allowed_origin_for_testing(const GURL& origin) {
    allowed_origin_ = origin;
  }

  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::string device_name_;
  GURL allowed_origin_;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
