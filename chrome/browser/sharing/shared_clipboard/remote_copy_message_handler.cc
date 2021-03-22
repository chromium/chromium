// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/proto/remote_copy_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "skia/ext/image_operations.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if defined(OS_WIN)
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#endif  // defined(OS_WIN)

namespace {
constexpr size_t kMaxImageDownloadSize = 5 * 1024 * 1024;

// These values are the 2x of the preferred width and height defined in
// message_center_constants.h, which are in dip.
constexpr int kNotificationImageMaxWidthPx = 720;
constexpr int kNotificationImageMaxHeightPx = 480;

// The initial delay for the timer that detects clipboard writes. An exponential
// backoff will double this value whenever the OneShotTimer reschedules.
constexpr base::TimeDelta kInitialDetectionTimerDelay =
    base::TimeDelta::FromMilliseconds(1);

// Interval at which to update the progress notification for image downloads.
constexpr base::TimeDelta kImageDownloadUpdateProgressInterval =
    base::TimeDelta::FromMilliseconds(250);

// This method should be called on a ThreadPool thread because it performs a
// potentially slow operation.
SkBitmap ResizeImage(const SkBitmap& image, int width, int height) {
  TRACE_EVENT2("sharing", "ResizeImage", "src_pixels",
               image.width() * image.height(), "dst_pixels", width * height);
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, width, height);
}

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

std::u16string GetTextNotificationTitle(const std::string& device_name) {
  return device_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT_UNKNOWN_DEVICE)
             : l10n_util::GetStringFUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                   base::UTF8ToUTF16(device_name));
}

std::u16string GetImageNotificationTitle(const std::string& device_name) {
  return device_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT_UNKNOWN_DEVICE)
             : l10n_util::GetStringFUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT,
                   base::UTF8ToUTF16(device_name));
}

std::u16string GetRemainingTimeString(int64_t current,
                                      int64_t total,
                                      base::TimeDelta elapsed) {
  if (total <= 0)
    return std::u16string();

  int64_t elapsed_ms = elapsed.InMilliseconds();
  int64_t bytes_per_second = elapsed_ms == 0 ? 0 : current * 1000 / elapsed_ms;
  int64_t remaining_bytes = total - current;
  base::TimeDelta remaining_time =
      base::TimeDelta::FromSeconds(remaining_bytes / bytes_per_second);

  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                ui::TimeFormat::LENGTH_SHORT, remaining_time);
}

std::u16string GetProgressString(int64_t current, int64_t total) {
  ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
  std::u16string current_string =
      ui::FormatBytesWithUnits(current, amount_units, /*show_units=*/false);
  std::u16string total_string =
      ui::FormatBytesWithUnits(total, amount_units, /*show_units=*/true);

  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_SIZES, current_string,
                                    total_string);
}

bool CanUpdateProgressNotification() {
#if defined(OS_WIN)
  // TODO(crbug.com/1064558): Windows system notifications don't support updates
  // so only show the initial progress notification and replace it with the
  // final image notification at the end.
  if (NotificationPlatformBridgeWin::SystemNotificationEnabled())
    return false;
#endif  // defined(OS_WIN)
  return true;
}

}  // namespace

RemoteCopyMessageHandler::RemoteCopyMessageHandler(Profile* profile)
    : profile_(profile) {}

RemoteCopyMessageHandler::~RemoteCopyMessageHandler() = default;

void RemoteCopyMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    DoneCallback done_callback) {
  DCHECK(message.has_remote_copy_message());
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::OnMessage");

  // First cancel any pending async tasks that might otherwise overwrite the
  // results of the more recent message.
  CancelAsyncTasks();

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
  TRACE_EVENT1("sharing", "RemoteCopyMessageHandler::HandleText", "text_size",
               text.size());

  if (text.empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureEmptyText);
    return;
  }

  LogRemoteCopyReceivedTextSize(text.size());

  uint64_t old_sequence_number =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  base::ElapsedTimer write_timer;
  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));
  }
  LogRemoteCopyWriteTime(write_timer.Elapsed(), /*is_image=*/false);
  // Unretained(this) is safe here because |this| owns |write_detection_timer_|.
  write_detection_timer_.Start(
      FROM_HERE, kInitialDetectionTimerDelay,
      base::BindOnce(&RemoteCopyMessageHandler::DetectWrite,
                     base::Unretained(this), old_sequence_number,
                     base::TimeTicks::Now(), /*is_image=*/false));
  notification_id_ = base::GenerateGUID();
  ShowNotification(GetTextNotificationTitle(device_name_), SkBitmap());
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledText);
}

void RemoteCopyMessageHandler::HandleImage(const std::string& image_url) {
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::HandleImage");

  GURL url(image_url);

  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageUrlNotTrustworthy);
    return;
  }

  if (!IsImageSourceAllowed(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageOriginNotAllowed);
    return;
  }

  bool should_show_progress =
      base::FeatureList::IsEnabled(kRemoteCopyProgressNotification);
  bool can_update_notification = CanUpdateProgressNotification();

  if (should_show_progress) {
    ClearProgressAndCloseNotification();
    UpdateProgressNotification(l10n_util::GetStringUTF16(
        can_update_notification
            ? IDS_SHARING_REMOTE_COPY_NOTIFICATION_PREPARING_DOWNLOAD
            : IDS_SHARING_REMOTE_COPY_NOTIFICATION_PROCESSING_IMAGE));
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  // This request should be unauthenticated (no cookies), and shouldn't be
  // stored in the cache (this URL is only fetched once, ever.)
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  timer_ = base::ElapsedTimer();
  // Unretained(this) is safe here because |this| owns |url_loader_|.
  if (should_show_progress && can_update_notification) {
    url_loader_->SetOnResponseStartedCallback(
        base::BindOnce(&RemoteCopyMessageHandler::OnImageResponseStarted,
                       base::Unretained(this)));
    url_loader_->SetOnDownloadProgressCallback(
        base::BindRepeating(&RemoteCopyMessageHandler::OnImageDownloadProgress,
                            base::Unretained(this)));
  }
  url_loader_->DownloadToString(
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&RemoteCopyMessageHandler::OnURLLoadComplete,
                     base::Unretained(this)),
      kMaxImageDownloadSize);
}

bool RemoteCopyMessageHandler::IsImageSourceAllowed(const GURL& image_url) {
  std::vector<std::string> parts =
      base::SplitString(kRemoteCopyAllowedOrigins.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& part : parts) {
    GURL allowed_origin(part);
    // The actual image URL may have a hash in the subdomain. This means we
    // cannot match the entire host - we'll match the domain instead.
    if (image_url.SchemeIs(allowed_origin.scheme_piece()) &&
        image_url.DomainIs(allowed_origin.host_piece()) &&
        image_url.EffectiveIntPort() == allowed_origin.EffectiveIntPort()) {
      return true;
    }
  }
  return false;
}

void RemoteCopyMessageHandler::OnImageResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  image_content_length_ = response_head.content_length;
}

void RemoteCopyMessageHandler::OnImageDownloadProgress(uint64_t current) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  image_content_progress_ = current;
}

void RemoteCopyMessageHandler::UpdateProgressNotification(
    const std::u16string& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (notification_id_.empty()) {
    notification_id_ = base::GenerateGUID();
    // base::Unretained is safe as the SharingService owns |this| via the
    // SharingHandlerRegistry and also the passed callback.
    SharingServiceFactory::GetForBrowserContext(profile_)
        ->SetNotificationActionHandler(
            notification_id_,
            base::BindRepeating(
                &RemoteCopyMessageHandler::OnProgressNotificationAction,
                base::Unretained(this)));
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &kSendTabToSelfIcon;
  rich_notification_data.never_timeout = true;

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id_,
      GetImageNotificationTitle(device_name_),
      GetRemainingTimeString(image_content_progress_, image_content_length_,
                             timer_.Elapsed()),
      /*icon=*/gfx::Image(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      rich_notification_data,
      /*delegate=*/nullptr);

  std::vector<message_center::ButtonInfo> notification_actions;
  message_center::ButtonInfo button_info =
      message_center::ButtonInfo(l10n_util::GetStringUTF16(IDS_CANCEL));
  notification_actions.push_back(button_info);
  notification.set_buttons(notification_actions);

  if (image_content_length_ <= 0) {
    // TODO(knollr): Show transfer status if |image_content_progress_| is != 0.
    // This might happen if we don't know the total size of the image but we
    // still want to show how many bytes have been transferred.
    notification.set_progress(-1);
#if defined(OS_MAC)
    // On macOS we only have the title and message available. The progress is
    // prepended to the title and the message should be the context.
    notification.set_message(context);
#else
    notification.set_progress_status(context);
#endif  // defined(OS_MAC)
  } else {
    notification.set_progress(image_content_progress_ * 100 /
                              image_content_length_);
    std::u16string progress =
        GetProgressString(image_content_progress_, image_content_length_);
#if defined(OS_MAC)
    // On macOS we only have the title and message available. The progress is
    // prepended to the title and the message should be the progress.
    notification.set_message(progress);
#else
    notification.set_progress_status(progress);
#endif  // defined(OS_MAC)
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SHARING, notification, /*metadata=*/nullptr);

  if (!CanUpdateProgressNotification())
    return;

  // Unretained(this) is safe here because |this| owns
  // |image_download_update_progress_timer_|.
  image_download_update_progress_timer_.Start(
      FROM_HERE, kImageDownloadUpdateProgressInterval,
      base::BindOnce(&RemoteCopyMessageHandler::UpdateProgressNotification,
                     base::Unretained(this), context));
}

void RemoteCopyMessageHandler::ClearProgressAndCloseNotification() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  image_content_length_ = -1;
  image_content_progress_ = 0;
  progress_notification_closed_ = false;

  if (notification_id_.empty())
    return;

  SharingServiceFactory::GetForBrowserContext(profile_)
      ->SetNotificationActionHandler(notification_id_, base::NullCallback());
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::SHARING, notification_id_);

  notification_id_.clear();
}

void RemoteCopyMessageHandler::OnProgressNotificationAction(
    base::Optional<int> button,
    bool closed) {
  // Clicks on the progress notification body are ignored.
  if (!closed && !button)
    return;

  // Stop updating the progress notification.
  image_download_update_progress_timer_.AbandonAndStop();

  // Let the download continue if the notification was dismissed.
  if (closed) {
    // Remove the handler as this notification is now closed.
    SharingServiceFactory::GetForBrowserContext(profile_)
        ->SetNotificationActionHandler(notification_id_, base::NullCallback());
    progress_notification_closed_ = true;
    return;
  }

  // Cancel the download if the cancel button was pressed.
  DCHECK_EQ(0, *button);
  CancelAsyncTasks();
}

void RemoteCopyMessageHandler::OnURLLoadComplete(
    std::unique_ptr<std::string> content) {
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::OnURLLoadComplete");

  if (!progress_notification_closed_ && CanUpdateProgressNotification()) {
    image_content_length_ = -1;
    UpdateProgressNotification(l10n_util::GetStringUTF16(
        IDS_SHARING_REMOTE_COPY_NOTIFICATION_PROCESSING_IMAGE));
  }

  image_download_update_progress_timer_.AbandonAndStop();

  int code;
  if (url_loader_->NetError() != net::OK) {
    code = url_loader_->NetError();
  } else if (!url_loader_->ResponseInfo() ||
             !url_loader_->ResponseInfo()->headers) {
    code = net::OK;
  } else {
    code = url_loader_->ResponseInfo()->headers->response_code();
  }
  LogRemoteCopyLoadImageStatusCode(code);

  url_loader_.reset();
  if (!content || content->empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureNoImageContentLoaded);
    return;
  }

  LogRemoteCopyLoadImageTime(timer_.Elapsed());
  LogRemoteCopyReceivedImageSizeBeforeDecode(content->size());

  timer_ = base::ElapsedTimer();
  ImageDecoder::Start(this, *content);
}

void RemoteCopyMessageHandler::OnImageDecoded(const SkBitmap& image) {
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::OnImageDecoded");

  if (image.drawsNothing()) {
    Finish(RemoteCopyHandleMessageResult::kFailureDecodedImageDrawsNothing);
    return;
  }

  LogRemoteCopyDecodeImageTime(timer_.Elapsed());
  LogRemoteCopyReceivedImageSizeAfterDecode(image.computeByteSize());

  if (!base::FeatureList::IsEnabled(kRemoteCopyImageNotification)) {
    WriteImageAndShowNotification(image, image);
    return;
  }

  double scale = std::min(
      static_cast<double>(kNotificationImageMaxWidthPx) / image.width(),
      static_cast<double>(kNotificationImageMaxHeightPx) / image.height());

  // If the image is too large to show in a notification, resize it first.
  if (scale < 1.0) {
    int resized_width =
        base::ClampToRange(static_cast<int>(scale * image.width()), 0,
                           kNotificationImageMaxWidthPx);
    int resized_height =
        base::ClampToRange(static_cast<int>(scale * image.height()), 0,
                           kNotificationImageMaxHeightPx);

    // Unretained(this) is safe here because |this| owns |resize_callback_|.
    resize_callback_.Reset(
        base::BindOnce(&RemoteCopyMessageHandler::WriteImageAndShowNotification,
                       base::Unretained(this), image));
    timer_ = base::ElapsedTimer();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ResizeImage, image, resized_width, resized_height),
        resize_callback_.callback());
  } else {
    WriteImageAndShowNotification(image, image);
  }
}

void RemoteCopyMessageHandler::OnDecodeImageFailed() {
  Finish(RemoteCopyHandleMessageResult::kFailureDecodeImageFailed);
}

void RemoteCopyMessageHandler::WriteImageAndShowNotification(
    const SkBitmap& original_image,
    const SkBitmap& resized_image) {
  TRACE_EVENT1("sharing",
               "RemoteCopyMessageHandler::WriteImageAndShowNotification",
               "bytes", original_image.computeByteSize());

  if (original_image.dimensions() != resized_image.dimensions())
    LogRemoteCopyResizeImageTime(timer_.Elapsed());

  uint64_t old_sequence_number =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  base::ElapsedTimer write_timer;
  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteImage(original_image);
  }
  LogRemoteCopyWriteTime(write_timer.Elapsed(), /*is_image=*/true);
  // Unretained(this) is safe here because |this| owns |write_detection_timer_|.
  write_detection_timer_.Start(
      FROM_HERE, kInitialDetectionTimerDelay,
      base::BindOnce(&RemoteCopyMessageHandler::DetectWrite,
                     base::Unretained(this), old_sequence_number,
                     base::TimeTicks::Now(), /*is_image=*/true));

#if defined(OS_MAC)
  // On macOS we can't replace a persistent notification with a non-persistent
  // one because they are posted from different sources (app vs xpc). To avoid
  // having both notifications on screen, remove the progress one first.
  if (!progress_notification_closed_ &&
      !base::FeatureList::IsEnabled(kRemoteCopyPersistentNotification)) {
    ClearProgressAndCloseNotification();
  }
#endif  // defined(OS_MAC)

  // If the notification id is not empty there must be a progress notification
  // that can be updated. Just clear its action handler.
  if (!notification_id_.empty()) {
    SharingServiceFactory::GetForBrowserContext(profile_)
        ->SetNotificationActionHandler(notification_id_, base::NullCallback());
  } else {
    notification_id_ = base::GenerateGUID();
  }

  ShowNotification(GetImageNotificationTitle(device_name_), resized_image);
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledImage);
}

void RemoteCopyMessageHandler::ShowNotification(const std::u16string& title,
                                                const SkBitmap& image) {
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::ShowNotification");

  gfx::Image icon;
  message_center::RichNotificationData rich_notification_data;

  bool use_image_notification =
      base::FeatureList::IsEnabled(kRemoteCopyImageNotification) &&
      !image.drawsNothing();

  if (use_image_notification) {
#if defined(OS_MAC)
    // On macOS notifications do not support large images so use the icon
    // instead.
    icon = gfx::Image::CreateFrom1xBitmap(image);
#else
    rich_notification_data.image = gfx::Image::CreateFrom1xBitmap(image);
#endif  // defined(OS_MAC)
  }

  rich_notification_data.vector_small_image = &kSendTabToSelfIcon;
  rich_notification_data.never_timeout =
      base::FeatureList::IsEnabled(kRemoteCopyPersistentNotification);

  message_center::NotificationType type =
      use_image_notification ? message_center::NOTIFICATION_TYPE_IMAGE
                             : message_center::NOTIFICATION_TYPE_SIMPLE;

  ui::Accelerator paste_accelerator(ui::VKEY_V, ui::EF_PLATFORM_ACCELERATOR);

  message_center::Notification notification(
      type, notification_id_, title,
      l10n_util::GetStringFUTF16(
          IDS_SHARING_REMOTE_COPY_NOTIFICATION_DESCRIPTION,
          paste_accelerator.GetShortcutText()),
      icon,
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      rich_notification_data,
      /*delegate=*/nullptr);

  if (!CanUpdateProgressNotification())
    notification.set_renotify(true);

  // Make the notification silent if we're replacing a progress notification.
  bool should_show_progress =
      base::FeatureList::IsEnabled(kRemoteCopyProgressNotification);
  if (should_show_progress && !progress_notification_closed_)
    notification.set_silent(true);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SHARING, notification, /*metadata=*/nullptr);
}

void RemoteCopyMessageHandler::DetectWrite(uint64_t old_sequence_number,
                                           base::TimeTicks start_ticks,
                                           bool is_image) {
  TRACE_EVENT0("sharing", "RemoteCopyMessageHandler::DetectWrite");

  uint64_t current_sequence_number =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_ticks;
  if (current_sequence_number != old_sequence_number) {
    LogRemoteCopyWriteDetectionTime(elapsed, is_image);
    return;
  }

  if (elapsed > base::TimeDelta::FromSeconds(10))
    return;

  // Unretained(this) is safe here because |this| owns |write_detection_timer_|.
  base::TimeDelta backoff_delay = write_detection_timer_.GetCurrentDelay() * 2;
  write_detection_timer_.Start(
      FROM_HERE, backoff_delay,
      base::BindOnce(&RemoteCopyMessageHandler::DetectWrite,
                     base::Unretained(this), old_sequence_number, start_ticks,
                     is_image));
}

void RemoteCopyMessageHandler::Finish(RemoteCopyHandleMessageResult result) {
  TRACE_EVENT1("sharing", "RemoteCopyMessageHandler::Finish", "result", result);

  if (result != RemoteCopyHandleMessageResult::kSuccessHandledText &&
      result != RemoteCopyHandleMessageResult::kSuccessHandledImage) {
    ClearProgressAndCloseNotification();
  }

  LogRemoteCopyHandleMessageResult(result);
  device_name_.clear();
}

void RemoteCopyMessageHandler::CancelAsyncTasks() {
  url_loader_.reset();
  ImageDecoder::Cancel(this);
  resize_callback_.Cancel();
  write_detection_timer_.AbandonAndStop();
  image_download_update_progress_timer_.AbandonAndStop();
  ClearProgressAndCloseNotification();
}
