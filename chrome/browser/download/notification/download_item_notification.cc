// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_item_notification.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/notification/download_notification_manager.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

using base::UserMetricsAction;
using offline_items_collection::FailState;

namespace {

const char kDownloadNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

const char kDownloadNotificationOrigin[] = "chrome://downloads";

// Background color of the preview images
const SkColor kImageBackgroundColor = SK_ColorWHITE;

// Maximum size of preview image. If the image exceeds this size, don't show the
// preview image.
const int64_t kMaxImagePreviewSize = 10 * 1024 * 1024;  // 10 MB

std::string ReadNotificationImage(const base::FilePath& file_path) {
  std::string data;
  bool ret = base::ReadFileToString(file_path, &data);
  if (!ret)
    return std::string();

  DCHECK_LE(data.size(), static_cast<size_t>(kMaxImagePreviewSize));

  return data;
}

SkBitmap CropImage(const SkBitmap& original_bitmap) {
  DCHECK_NE(0, original_bitmap.width());
  DCHECK_NE(0, original_bitmap.height());

  const SkSize container_size =
      SkSize::Make(message_center::kNotificationPreferredImageWidth,
                   message_center::kNotificationPreferredImageHeight);
  const float container_aspect_ratio =
      static_cast<float>(message_center::kNotificationPreferredImageWidth) /
      message_center::kNotificationPreferredImageHeight;
  const float image_aspect_ratio =
      static_cast<float>(original_bitmap.width()) / original_bitmap.height();

  SkRect source_rect;
  if (image_aspect_ratio > container_aspect_ratio) {
    float width = original_bitmap.height() * container_aspect_ratio;
    source_rect = SkRect::MakeXYWH((original_bitmap.width() - width) / 2, 0,
                                   width, original_bitmap.height());
  } else {
    float height = original_bitmap.width() / container_aspect_ratio;
    source_rect = SkRect::MakeXYWH(0, (original_bitmap.height() - height) / 2,
                                   original_bitmap.width(), height);
  }

  SkBitmap container_bitmap;
  container_bitmap.allocN32Pixels(container_size.width(),
                                  container_size.height());
  SkSamplingOptions sampling({1.0f / 3, 1.0f / 3});
  SkCanvas container_image(container_bitmap);
  container_image.drawColor(kImageBackgroundColor);
  container_image.drawImageRect(original_bitmap.asImage(), source_rect,
                                SkRect::MakeSize(container_size), sampling,
                                nullptr, SkCanvas::kStrict_SrcRectConstraint);

  return container_bitmap;
}

void RecordButtonClickAction(DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_ShowInFolder"));
      break;
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_OpenWhenComplete"));
      break;
    case DownloadCommands::CANCEL:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_Cancel"));
      break;
    case DownloadCommands::DISCARD:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_Discard"));
      break;
    case DownloadCommands::KEEP:
      base::RecordAction(UserMetricsAction("DownloadNotification.Button_Keep"));
      break;
    case DownloadCommands::LEARN_MORE_SCANNING:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_LearnScanning"));
      break;
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_LearnMixedContent"));
      break;
    case DownloadCommands::PAUSE:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_Pause"));
      break;
    case DownloadCommands::RESUME:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_Resume"));
      break;
    case DownloadCommands::COPY_TO_CLIPBOARD:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_CopyToClipboard"));
      break;
    case DownloadCommands::DEEP_SCAN:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_DeepScan"));
      break;
    case DownloadCommands::REVIEW:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_Review"));
      break;
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_OpenWithMediaApp"));
      return;
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Button_EditWithMediaApp"));
      return;
    // Not actually displayed in notification, so should never be reached.
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::RETRY:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool IsExtensionDownload(DownloadUIModel* item) {
  return item->GetDownloadItem() &&
         download_crx_util::IsExtensionDownload(*item->GetDownloadItem());
}

}  // namespace

DownloadItemNotification::DownloadItemNotification(
    Profile* profile,
    DownloadUIModel::DownloadUIModelPtr item)
    : profile_(profile), item_(std::move(item)) {
  item_->SetDelegate(this);
  // Creates the notification instance. |title|, |body| and |icon| will be
  // overridden by UpdateNotificationData() below.
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.should_make_spoken_feedback_for_popup_updates = false;
  rich_notification_data.vector_small_image =
      &vector_icons::kNotificationDownloadIcon;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, GetNotificationId(),
      std::u16string(),  // title
      std::u16string(),  // body
      ui::ImageModel(),  // icon
      l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_NOTIFICATION_DISPLAY_SOURCE),  // display_source
      GURL(kDownloadNotificationOrigin),              // origin_url
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kDownloadNotificationNotifierId,
          ash::NotificationCatalogName::kDownloadNotification),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDownloadNotificationNotifierId),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));
  notification_->set_progress(0);
  notification_->set_fullscreen_visibility(
      message_center::FullscreenVisibility::OVER_USER);
  Update();
}

DownloadItemNotification::~DownloadItemNotification() {
  if (image_decode_status_ == IN_PROGRESS)
    ImageDecoder::Cancel(this);
}

void DownloadItemNotification::SetObserver(Observer* observer) {
  observer_ = observer;
}

DownloadUIModel* DownloadItemNotification::GetDownload() {
  return item_.get();
}

void DownloadItemNotification::OnDownloadUpdated() {
  Update();
}

void DownloadItemNotification::OnDownloadDestroyed(const ContentId& id) {
  item_.reset();
  NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, id.id);
  // |this| will be deleted before there's a chance for Close() to be called
  // through the delegate, so preemptively call it now.
  Close(false);

  // This object may get deleted after this call.
  observer_->OnDownloadDestroyed(id);
}

void DownloadItemNotification::DisablePopup() {
  if (notification_->priority() == message_center::LOW_PRIORITY)
    return;

  // Hides a notification from popup notifications if it's a pop-up, by
  // decreasing its priority and reshowing itself. Low-priority notifications
  // doesn't pop-up itself so this logic works as disabling pop-up.
  CloseNotification();
  notification_->set_priority(message_center::LOW_PRIORITY);
  closed_ = false;
  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void DownloadItemNotification::Close(bool by_user) {
  closed_ = true;

  if (item_ && item_->IsDangerous() && !item_->IsDone()) {
    base::RecordAction(
        UserMetricsAction("DownloadNotification.Close_Dangerous"));
    item_->Cancel(by_user);
    return;
  }

  if (item_ && item_->IsInsecure() && !item_->IsDone()) {
    item_->Cancel(by_user);
    return;
  }

  if (image_decode_status_ == IN_PROGRESS) {
    image_decode_status_ = NOT_STARTED;
    ImageDecoder::Cancel(this);
  }
}

void DownloadItemNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!item_)
    return;

  if (button_index) {
    if (*button_index < 0 ||
        static_cast<size_t>(*button_index) >= button_actions_->size()) {
      // Out of boundary.
      NOTREACHED_IN_MIGRATION();
      return;
    }

    DownloadCommands::Command command = button_actions_->at(*button_index);
    RecordButtonClickAction(command);

    // Completing Safe Browsing scan early if requested to open.
    if (IsScanning() && AllowedToOpenWhileScanning() &&
        command == DownloadCommands::OPEN_WHEN_COMPLETE) {
      item_->CompleteSafeBrowsingScan();
    }

    DownloadCommands(item_->GetWeakPtr()).ExecuteCommand(command);

    // After DISCARD, `this` has been destroyed.
    if (command == DownloadCommands::DISCARD) {
      return;
    }

    // ExecuteCommand() might cause |item_| to be destroyed.
    if (item_ && command != DownloadCommands::PAUSE &&
        command != DownloadCommands::RESUME &&
        command != DownloadCommands::REVIEW) {
      CloseNotification();
    }

    // Shows the notification again after clicking "Keep" on dangerous download.
    if (command == DownloadCommands::KEEP) {
      show_next_ = true;
      Update();
    }

    if (command == DownloadCommands::REVIEW) {
      content::WebContents* contents =
          GetBrowser()->tab_strip_model()->GetActiveWebContents();

      // If there is no currently active web contents, just show the user the
      // downloads page so they get more context on the warned download needing
      // to be reviewed.
      // TODO(b/285119059): Expand this solution by having the review dialog
      // also open immediately after the download page is available.
      if (!contents) {
        chrome::ShowDownloads(GetBrowser());
        return;
      }

      item_->ReviewScanningVerdict(contents);
      in_review_ = true;
      Update();
    }

    return;
  }

  // Handle a click on the notification's body.
  if (item_->IsDangerous()) {
    base::RecordAction(
        UserMetricsAction("DownloadNotification.Click_Dangerous"));
    // Do nothing.
    return;
  }

  // Handle a click on the notification's body.
  if (item_->IsInsecure()) {
    chrome::ShowDownloads(GetBrowser());
    return;
  }

  // Handle a click on the notification's body while scanning.
  if (IsScanning() && AllowedToOpenWhileScanning()) {
    item_->CompleteSafeBrowsingScan();
    item_->OpenDownload();
    return;
  }

  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Click_InProgress"));
      item_->SetOpenWhenComplete(!item_->GetOpenWhenComplete());  // Toggle
      break;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Click_Stopped"));
      chrome::ShowDownloads(GetBrowser());
      CloseNotification();
      break;
    case download::DownloadItem::COMPLETE:
      base::RecordAction(
          UserMetricsAction("DownloadNotification.Click_Completed"));
      item_->OpenDownload();
      CloseNotification();
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
}

std::string DownloadItemNotification::GetNotificationId() const {
  return item_->GetContentId().id;
}

void DownloadItemNotification::CloseNotification() {
  if (closed_)
    return;

  NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationId());
}

void DownloadItemNotification::Update() {
  if (!item_)
    return;

  auto download_state = item_->GetState();

  // When the download is just completed, interrupted or transitions to
  // dangerous, make sure it pops up again.
  bool pop_up =
      ((item_->IsDangerous() && !previous_dangerous_state_) ||
       (item_->IsInsecure() && !previous_insecure_state_) ||
       (download_state == download::DownloadItem::COMPLETE &&
        previous_download_state_ != download::DownloadItem::COMPLETE) ||
       (download_state == download::DownloadItem::INTERRUPTED &&
        previous_download_state_ != download::DownloadItem::INTERRUPTED));
  UpdateNotificationData(!closed_ || show_next_ || pop_up, pop_up);

  show_next_ = false;
  previous_download_state_ = item_->GetState();
  previous_dangerous_state_ = item_->IsDangerous();
  previous_insecure_state_ = item_->IsInsecure();
}

void DownloadItemNotification::UpdateNotificationData(bool display,
                                                      bool force_pop_up) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (item_->GetState() == download::DownloadItem::CANCELLED) {
    // Confirms that a download is cancelled by user action.
    DCHECK(item_->GetLastFailState() == FailState::USER_CANCELED ||
           item_->GetLastFailState() == FailState::USER_SHUTDOWN);

    CloseNotification();
    return;
  }

  DownloadCommands command(item_->GetWeakPtr());

  notification_->set_title(GetTitle());
  notification_->set_message(GetSubStatusString());
  notification_->set_progress_status(GetStatusString());

  if (item_->IsDangerous()) {
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    MaybeRecordDangerousDownloadWarningShown(*item_);
    if (!item_->MightBeMalicious() &&
        item_->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
      notification_->set_priority(message_center::HIGH_PRIORITY);
    } else {
      notification_->set_priority(message_center::DEFAULT_PRIORITY);
    }
  } else if (item_->IsInsecure()) {
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    switch (item_->GetInsecureDownloadStatus()) {
      case download::DownloadItem::InsecureDownloadStatus::BLOCK:
        notification_->set_priority(message_center::HIGH_PRIORITY);
        break;
      case download::DownloadItem::InsecureDownloadStatus::WARN:
        notification_->set_priority(message_center::DEFAULT_PRIORITY);
        break;
      case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
      case download::DownloadItem::InsecureDownloadStatus::SAFE:
      case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
      case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    switch (item_->GetState()) {
      case download::DownloadItem::IN_PROGRESS: {
        int percent_complete = item_->PercentComplete();
        // Show "running" progress when percent is unknown or during cloud scan.
        if (percent_complete >= 0 && !IsScanning()) {
          notification_->set_progress(percent_complete);
        } else {
          // Negative progress value shows an indeterminate progress bar.
          notification_->set_progress(-1);
        }

        notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        break;
      }
      case download::DownloadItem::COMPLETE:
        DCHECK(item_->IsDone());
        notification_->set_priority(message_center::DEFAULT_PRIORITY);
        notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
        notification_->set_progress(100);
        break;
      case download::DownloadItem::CANCELLED:
        // Handled above.
        NOTREACHED_IN_MIGRATION();
        return;
      case download::DownloadItem::INTERRUPTED:
        // Shows a notifiation as progress type once so the visible content will
        // be updated. (same as the case of type = COMPLETE)
        notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
        notification_->set_progress(0);
        notification_->set_priority(message_center::DEFAULT_PRIORITY);
        break;
      case download::DownloadItem::MAX_DOWNLOAD_STATE:  // sentinel
        NOTREACHED_IN_MIGRATION();
    }
  }
  SkColor notification_color = GetNotificationIconColor();
  ui::ColorId color_id = cros_tokens::kCrosSysPrimary;
  switch (notification_color) {
    case ash::kSystemNotificationColorNormal:
      color_id = cros_tokens::kCrosSysPrimary;
      break;
    case ash::kSystemNotificationColorWarning:
      color_id = cros_tokens::kCrosSysWarning;
      break;
    case ash::kSystemNotificationColorCriticalWarning:
      color_id = cros_tokens::kCrosSysError;
      break;
  }
  notification_->set_accent_color_id(color_id);

  std::vector<message_center::ButtonInfo> notification_actions;
  std::unique_ptr<std::vector<DownloadCommands::Command>> actions(
      GetExtraActions());

  button_actions_ = std::make_unique<std::vector<DownloadCommands::Command>>();
  for (auto it = actions->begin(); it != actions->end(); it++) {
    button_actions_->push_back(*it);
    message_center::ButtonInfo button_info =
        message_center::ButtonInfo(GetCommandLabel(*it));
    notification_actions.push_back(button_info);
  }
  notification_->set_buttons(notification_actions);

  notification_->set_renotify(force_pop_up);

  if (display) {
    closed_ = false;
    NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
        NotificationHandler::Type::TRANSIENT, *notification_,
        /*metadata=*/nullptr);
  }

  if (item_->IsDone() && image_decode_status_ == NOT_STARTED) {
    // TODO(yoshiki): Add an UMA to collect statistics of image file sizes.

    if (item_->GetCompletedBytes() > kMaxImagePreviewSize)
      return;

    DCHECK(notification_->image().IsEmpty());

    image_decode_status_ = IN_PROGRESS;

    if (item_->HasSupportedImageMimeType()) {
      base::FilePath file_path = item_->GetFullPath();
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&ReadNotificationImage, file_path),
          base::BindOnce(&DownloadItemNotification::OnImageLoaded,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

SkColor DownloadItemNotification::GetNotificationIconColor() {
  if (item_->IsDangerous()) {
    return (item_->MightBeMalicious() &&
            item_->GetDangerType() !=
                download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING)
               ? ash::kSystemNotificationColorCriticalWarning
               : ash::kSystemNotificationColorWarning;
  }
  if (item_->IsInsecure()) {
    switch (item_->GetInsecureDownloadStatus()) {
      case download::DownloadItem::InsecureDownloadStatus::BLOCK:
        return ash::kSystemNotificationColorCriticalWarning;
      case download::DownloadItem::InsecureDownloadStatus::WARN:
        return ash::kSystemNotificationColorWarning;
      case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
      case download::DownloadItem::InsecureDownloadStatus::SAFE:
      case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
      case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::COMPLETE:
      return ash::kSystemNotificationColorNormal;

    case download::DownloadItem::INTERRUPTED:
      return ash::kSystemNotificationColorCriticalWarning;

    case download::DownloadItem::CANCELLED:
      break;

    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return gfx::kPlaceholderColor;
}

void DownloadItemNotification::OnImageLoaded(std::string image_data) {
  if (image_data.empty())
    return;

  // TODO(yoshiki): Set option to reduce the image size to supress memory usage.
  ImageDecoder::Start(this, std::move(image_data));
}

void DownloadItemNotification::OnImageDecoded(const SkBitmap& decoded_bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (decoded_bitmap.drawsNothing()) {
    OnDecodeImageFailed();
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CropImage, decoded_bitmap),
      base::BindOnce(&DownloadItemNotification::OnImageCropped,
                     weak_factory_.GetWeakPtr()));
}

void DownloadItemNotification::OnImageCropped(const SkBitmap& bitmap) {
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  notification_->SetImage(image);

// Provide the file path that backs the image to facilitate notification drag.
#if BUILDFLAG(IS_CHROMEOS)
  notification_->set_image_path(item_->GetFullPath());
#endif

  image_decode_status_ = DONE;
  UpdateNotificationData(!closed_, false);
}

void DownloadItemNotification::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(notification_->image().IsEmpty());

  image_decode_status_ = FAILED;
  UpdateNotificationData(!closed_, false);
}

std::unique_ptr<std::vector<DownloadCommands::Command>>
DownloadItemNotification::GetExtraActions() const {
  std::unique_ptr<std::vector<DownloadCommands::Command>> actions(
      new std::vector<DownloadCommands::Command>());

  if (item_->GetDangerType() ==
      download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    actions->push_back(DownloadCommands::DEEP_SCAN);
    actions->push_back(DownloadCommands::KEEP);
    return actions;
  }

  if (item_->IsDangerous()) {
    if (item_->GetDangerType() ==
        download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
      actions->push_back(DownloadCommands::LEARN_MORE_SCANNING);
    } else if (item_->GetDangerType() ==
                   download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
               item_->GetDangerType() ==
                   download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE) {
      actions->push_back(DownloadCommands::DISCARD);
      actions->push_back(DownloadCommands::KEEP);
    } else if (item_->GetDangerType() !=
               download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
      actions->push_back(DownloadCommands::DISCARD);
    } else {
      actions->push_back(DownloadCommands::DISCARD);

      // Only include a keep/review button if there isn't an extra review dialog
      // opened already.
      if (!in_review_) {
        if (enterprise_connectors::ShouldPromptReviewForDownload(
                profile(), item_->GetDownloadItem())) {
          actions->push_back(DownloadCommands::REVIEW);
        } else {
          actions->push_back(DownloadCommands::KEEP);
        }
      }
    }
    return actions;
  }

  if (item_->IsInsecure()) {
    switch (item_->GetInsecureDownloadStatus()) {
      case download::DownloadItem::InsecureDownloadStatus::BLOCK:
        actions->push_back(DownloadCommands::DISCARD);
        break;
      case download::DownloadItem::InsecureDownloadStatus::WARN:
        actions->push_back(DownloadCommands::KEEP);
        break;
      case download::DownloadItem::InsecureDownloadStatus::UNKNOWN:
      case download::DownloadItem::InsecureDownloadStatus::SAFE:
      case download::DownloadItem::InsecureDownloadStatus::VALIDATED:
      case download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    actions->push_back(DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD);
    return actions;
  }

  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      if (item_->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
        if (AllowedToOpenWhileScanning())
          actions->push_back(DownloadCommands::OPEN_WHEN_COMPLETE);
      } else if (!item_->IsPaused()) {
        actions->push_back(DownloadCommands::PAUSE);
      } else {
        actions->push_back(DownloadCommands::RESUME);
      }
      actions->push_back(DownloadCommands::CANCEL);
      break;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      if (item_->CanResume())
        actions->push_back(DownloadCommands::RESUME);
      break;
    case download::DownloadItem::COMPLETE: {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      std::optional<DownloadCommands::Command> command =
          item_->MaybeGetMediaAppAction();
      if (command) {
        actions->push_back(*command);
      }
#endif

      actions->push_back(DownloadCommands::SHOW_IN_FOLDER);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      // We disable this functionality for now as the usage is very low, the
      // feature gets re-written at this time and there is currently no secure
      // way to determine the caller on the Ash side as the dialog is still
      // active when |seat::SetSelection| is reached.
      if (!notification_->image().IsEmpty())
        actions->push_back(DownloadCommands::COPY_TO_CLIPBOARD);
#endif
      break;
    }
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  return actions;
}

std::u16string DownloadItemNotification::GetTitle() const {
  std::u16string title_text;

  if (item_->GetDangerType() ==
      download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    return l10n_util::GetStringUTF16(
        IDS_PROMPT_SEND_TO_SAFEBROWSING_DOWNLOAD_TITLE);
  }

  if (item_->IsDangerous()) {
    if (item_->MightBeMalicious() &&
        item_->GetDangerType() !=
            download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
      return l10n_util::GetStringUTF16(
          IDS_PROMPT_BLOCKED_MALICIOUS_DOWNLOAD_TITLE);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    }
  }

  if (item_->IsInsecure()) {
    return l10n_util::GetStringUTF16(
        IDS_PROMPT_BLOCKED_INSECURE_DOWNLOAD_TITLE);
  }

  std::u16string file_name =
      item_->GetFileNameToReportUser().LossyDisplayName();
  if (IsScanning()) {
    return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_SCAN_TITLE,
                                      file_name);
  }

  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      if (!item_->IsPaused()) {
        title_text = l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE, file_name);
      } else {
        title_text = l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_STATUS_PAUSED_TITLE, file_name);
      }
      break;
    case download::DownloadItem::COMPLETE:
      title_text =
          l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_COMPLETE_TITLE);
      break;
    case download::DownloadItem::INTERRUPTED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
      break;
    case download::DownloadItem::CANCELLED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  return title_text;
}

std::u16string DownloadItemNotification::GetCommandLabel(
    DownloadCommands::Command command) const {
  int id = -1;
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      if (item_ && !item_->IsDone() &&
          item_->GetDangerType() !=
              download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING)
        id = IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN_WHEN_COMPLETE;
      else
        id = IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN;
      break;
    case DownloadCommands::PAUSE:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_PAUSE;
      break;
    case DownloadCommands::RESUME:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_RESUME;
      break;
    case DownloadCommands::SHOW_IN_FOLDER:
      return item_->GetShowInFolderText();
    case DownloadCommands::DISCARD:
      id = IDS_DISCARD_DOWNLOAD;
      break;
    case DownloadCommands::KEEP:
      id = IDS_CONFIRM_DOWNLOAD;
      break;
    case DownloadCommands::CANCEL:
      id = IDS_DOWNLOAD_LINK_CANCEL;
      break;
    case DownloadCommands::LEARN_MORE_SCANNING:
      id = IDS_LEARN_MORE;
      break;
    case DownloadCommands::COPY_TO_CLIPBOARD:
      id = IDS_DOWNLOAD_NOTIFICATION_COPY_TO_CLIPBOARD;
      break;
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
      id = IDS_LEARN_MORE;
      break;
    case DownloadCommands::DEEP_SCAN:
      id = IDS_SCAN_DOWNLOAD;
      break;
    case DownloadCommands::REVIEW:
      id = IDS_REVIEW_DOWNLOAD;
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
      id = IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN;
      break;
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      id = IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN_AND_EDIT;
      break;
#else
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
#endif
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::RETRY:
      // Only for menu.
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
  CHECK_NE(id, -1);
  return l10n_util::GetStringUTF16(id);
}

std::u16string DownloadItemNotification::GetWarningStatusString() const {
  // Should only be called if IsDangerous() or IsInsecure().
  DCHECK(item_->IsDangerous() || item_->IsInsecure());
  std::u16string elided_filename =
      item_->GetFileNameToReportUser().LossyDisplayName();
  // If insecure, that warning is shown first.
  if (item_->IsInsecure()) {
    return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_INSECURE_BLOCKED,
                                      elided_filename);
  }
  switch (item_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL: {
      return l10n_util::GetStringUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
    }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
      if (IsExtensionDownload(item_.get())) {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
      } else {
        return l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                                          elided_filename);
      }
    }
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
                                        elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      bool requests_ap_verdicts =
          safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              profile())
              ->IsUnderAdvancedProtection();
      return l10n_util::GetStringFUTF16(
          requests_ap_verdicts
              ? IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT_IN_ADVANCED_PROTECTION
              : IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
          elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
                                        elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_BLOCKED_TOO_LARGE,
                                        elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED: {
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_DOWNLOAD_BLOCKED_PASSWORD_PROTECTED, elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING: {
      return l10n_util::GetStringUTF16(
          IDS_PROMPT_DOWNLOAD_SENSITIVE_CONTENT_WARNING);
    }
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK: {
      return l10n_util::GetStringUTF16(
          IDS_PROMPT_DOWNLOAD_SENSITIVE_CONTENT_BLOCKED);
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DEEP_SCANNING,
                                        elided_filename);
    }
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING: {
      // TODO(crbug.com/40074456): Implement UX for this danger type.
      DUMP_WILL_BE_NOTREACHED();
      break;
    }
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED: {
      return l10n_util::GetStringUTF16(IDS_PROMPT_DOWNLOAD_BLOCKED_SCAN_FAILED);
    }
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX: {
      break;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string DownloadItemNotification::GetInProgressSubStatusString() const {
  // "Paused"
  if (item_->IsPaused())
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);

  // "In progress" (scanning)
  if (IsScanning())
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_IN_PROGRESS_SHORT);

  base::TimeDelta time_remaining;
  // time_remaining is only known if the download isn't paused.
  bool time_remaining_known =
      (!item_->IsPaused() && item_->TimeRemaining(&time_remaining));

  // A download scheduled to be opened when complete.
  if (item_->GetOpenWhenComplete()) {
    // "Opening when complete"
    if (!time_remaining_known)
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE);

    // "Opening in 10 secs"
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_OPEN_IN,
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_SHORT, time_remaining));
  }

  // In progress download with known time left: "10 secs left"
  if (time_remaining_known) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                  ui::TimeFormat::LENGTH_SHORT, time_remaining);
  }

  // "In progress"
  if (item_->GetCompletedBytes() > 0)
    return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_IN_PROGRESS_SHORT);

  // "Starting..."
  return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING);
}

std::u16string DownloadItemNotification::GetSubStatusString() const {
  if (item_->IsInsecure() || item_->IsDangerous())
    return GetWarningStatusString();

  if (item_->GetDangerType() ==
      download::DownloadDangerType::
          DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS) {
    return l10n_util::GetStringUTF16(
        IDS_PROMPT_DOWNLOAD_DEEP_SCANNED_OPENED_DANGEROUS);
  }

  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      // The download is a CRX (app, extension, theme, ...) and it is being
      // unpacked and validated.
      if (item_->AllDataSaved() && IsExtensionDownload(item_.get())) {
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING);
      } else {
        return GetInProgressSubStatusString();
      }
    case download::DownloadItem::COMPLETE: {
      if (item_->GetFileExternallyRemoved()) {
        // If the file has been removed: "Removed"
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_REMOVED);
      } else {
        std::u16string file_name =
            item_->GetFileNameToReportUser().LossyDisplayName();
        base::i18n::AdjustStringForLocaleDirection(&file_name);
        return file_name;
      }
    }
    case download::DownloadItem::INTERRUPTED: {
      FailState fail_state = item_->GetLastFailState();
      if (fail_state != FailState::USER_CANCELED) {
        const auto interrupt_text = item_->GetInterruptDescription();
        DCHECK(!interrupt_text.empty());
        return interrupt_text;
      }
      [[fallthrough]];  // Same as download::DownloadItem::CANCELLED.
    }
    case download::DownloadItem::CANCELLED:
      // "Cancelled"
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED);

    default:
      NOTREACHED_IN_MIGRATION();
  }

  return std::u16string();
}

std::u16string DownloadItemNotification::GetStatusString() const {
  if (item_->IsDangerous() || item_->IsInsecure())
    return std::u16string();

  if (IsScanning()) {
    return l10n_util::GetStringFUTF16(
        IDS_PROMPT_DEEP_SCANNING_APP_DOWNLOAD,
        item_->GetFileNameToReportUser().LossyDisplayName());
  }

  // The hostname. (E.g.:"example.com" or "127.0.0.1")
  std::u16string host_name = url_formatter::FormatUrlForSecurityDisplay(
      item_->GetURL(), url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  bool show_size_ratio = true;
  switch (item_->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      // The download is a CRX (app, extension, theme, ...) and it is being
      // unpacked and validated.
      if (item_->AllDataSaved() && IsExtensionDownload(item_.get())) {
        show_size_ratio = false;
      }
      break;
    case download::DownloadItem::COMPLETE:
      // If the file has been removed: Removed
      if (item_->GetFileExternallyRemoved()) {
        show_size_ratio = false;
      } else {
        // Otherwise, the download should be completed.
        // "3.4 MB from example.com"
        std::u16string size = ui::FormatBytes(item_->GetCompletedBytes());
        return l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_NOTIFICATION_STATUS_COMPLETED, size, host_name);
      }
      break;
    default:
      break;
  }

  // Indication of progress (E.g.:"100/200 MB" or "100 MB"), or just the
  // received bytes if the |show_size_ratio| flag is false.
  std::u16string size = show_size_ratio
                            ? item_->GetProgressSizesString()
                            : ui::FormatBytes(item_->GetCompletedBytes());

  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_NOTIFICATION_STATUS_SHORT,
                                    size, host_name);
}

bool DownloadItemNotification::IsScanning() const {
  return item_ && item_->GetState() == download::DownloadItem::IN_PROGRESS &&
         item_->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
}

bool DownloadItemNotification::AllowedToOpenWhileScanning() const {
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile());
  return !service ||
         !service->DelayUntilVerdict(
             enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
}

Browser* DownloadItemNotification::GetBrowser() const {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

Profile* DownloadItemNotification::profile() const {
  return profile_;
}
