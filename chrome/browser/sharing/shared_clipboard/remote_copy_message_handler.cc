// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync/protocol/sharing_remote_copy_message.pb.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/origin.h"

namespace {
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_copy_message_handler",
                                        R"(
          semantics {
            sender: "RemoteCopyMessageHandler"
            description:
              "Fetches an image from a URL specified in an FCM message."
            trigger:
              "The user sent an image to this device from another device that "
              "they control."
            data:
              "An image URL, from a Google storage service like blobstore."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can disable this behavior by signing out of Chrome."
            policy_exception_justification:
              "Can be controlled via Chrome sign-in."
          })");
}  // namespace

RemoteCopyMessageHandler::RemoteCopyMessageHandler(Profile* profile)
    : profile_(profile) {}

RemoteCopyMessageHandler::~RemoteCopyMessageHandler() = default;

void RemoteCopyMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    DoneCallback done_callback) {
  DCHECK(message.has_remote_copy_message());

  // First cancel any pending async tasks that might otherwise overwrite the
  // results of the more recent message.
  url_loader_.reset();
  ImageDecoder::Cancel(this);

  device_name_ = message.sender_device_name();

  switch (message.remote_copy_message().content_case()) {
    case chrome_browser_sharing::RemoteCopyMessage::kText:
      HandleText(message.remote_copy_message().text());
      break;
    case chrome_browser_sharing::RemoteCopyMessage::kImageUrl:
      HandleImage(message.remote_copy_message().image_url());
      break;
    case chrome_browser_sharing::RemoteCopyMessage::CONTENT_NOT_SET:
      NOTREACHED();
      break;
  }

  std::move(done_callback).Run(/*response=*/nullptr);
}

void RemoteCopyMessageHandler::HandleText(const std::string& text) {
  if (text.empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureEmptyText);
    return;
  }

  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));
  }
  ShowNotification();
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledText);
}

void RemoteCopyMessageHandler::HandleImage(const std::string& image_url) {
  GURL url(image_url);

  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageUrlNotTrustworthy);
    return;
  }

  if (!IsOriginAllowed(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageOriginNotAllowed);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  // This request should be unauthenticated (no cookies), and shouldn't be
  // stored in the cache (this URL is only fetched once, ever.)
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  // TODO(mvanouwerkerk): Downloads > 1MB (kMaxBoundedStringDownloadSize).
  // Using Unretained(this) is safe here because this owns url_loader_.
  url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&RemoteCopyMessageHandler::OnURLLoadComplete,
                     base::Unretained(this)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

bool RemoteCopyMessageHandler::IsOriginAllowed(const GURL& image_url) {
  url::Origin image_origin = url::Origin::Create(image_url);
  std::vector<std::string> parts =
      base::SplitString(kRemoteCopyAllowedOrigins.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& part : parts) {
    url::Origin allowed_origin = url::Origin::Create(GURL(part));
    if (image_origin.IsSameOriginWith(allowed_origin))
      return true;
  }
  return false;
}

void RemoteCopyMessageHandler::OnURLLoadComplete(
    std::unique_ptr<std::string> content) {
  url_loader_.reset();
  if (!content || content->empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureNoImageContentLoaded);
    return;
  }

  ImageDecoder::Start(this, *content);
}

void RemoteCopyMessageHandler::OnImageDecoded(const SkBitmap& decoded_image) {
  if (decoded_image.drawsNothing()) {
    Finish(RemoteCopyHandleMessageResult::kFailureDecodedImageDrawsNothing);
    return;
  }

  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteImage(decoded_image);
  }
  ShowNotification();
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledImage);
}

void RemoteCopyMessageHandler::OnDecodeImageFailed() {
  Finish(RemoteCopyHandleMessageResult::kFailureDecodeImageFailed);
}

void RemoteCopyMessageHandler::ShowNotification() {
  std::string notification_id = base::GenerateGUID();

  // TODO(mvanouwerkerk): Adjust notification text and icon once we have mocks.
  base::string16 notification_title =
      device_name_.empty()
          ? l10n_util::GetStringUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE_UNKNOWN_DEVICE)
          : l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::UTF8ToUTF16(device_name_));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      notification_title,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_DESCRIPTION),
      /*icon=*/gfx::Image(),
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SHARING, notification, /*metadata=*/nullptr);
}

void RemoteCopyMessageHandler::Finish(RemoteCopyHandleMessageResult result) {
  LogRemoteCopyHandleMessageResult(result);
  device_name_.clear();
}
