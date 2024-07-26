// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

constexpr char kNearbyInProgressNotificationId[] =
    "chrome://nearby_share/in_progress";
constexpr char kNearbyTransferResultNotificationIdPrefix[] =
    "chrome://nearby_share/result/";
constexpr char kNearbyDeviceTryingToShareNotificationId[] =
    "chrome://nearby_share/nearby_device_trying_to_share";
constexpr char kNearbyVisibilityReminderNotificationId[] =
    "chrome://nearby_share/visibility_reminder";
constexpr char kNearbyNotifier[] = "nearby";

std::string CreateNotificationIdForShareTarget(
    const ShareTarget& share_target) {
    return std::string(kNearbyTransferResultNotificationIdPrefix) +
           share_target.id.ToString();
}

// Creates a default Nearby Share notification with empty content.
message_center::Notification CreateNearbyNotification(const std::string& id) {
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(),
      /*icon=*/ui::ImageModel(),
      features::IsNameEnabled()
          ? NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_SOURCE_PH)
          : l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNearbyNotifier,
                                 ash::NotificationCatalogName::kNearbyShare),
      /*optional_fields=*/{},
      /*delegate=*/nullptr);

  notification.set_accent_color_id(cros_tokens::kCrosSysPrimary);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsNameEnabled()) {
    notification.set_vector_small_image(kNearbyShareInternalIcon);
  } else {
    notification.set_vector_small_image(kNearbyShareIcon);
  }
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  notification.set_vector_small_image(kNearbyShareIcon);
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  notification.set_settings_button_handler(
      message_center::SettingsButtonHandler::DELEGATE);

  return notification;
}

std::string GetTimestampString() {
  return base::NumberToString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

FileAttachment::Type GetCommonFileAttachmentType(
    const std::vector<FileAttachment>& files) {
  if (files.empty())
    return FileAttachment::Type::kUnknown;

  FileAttachment::Type type = files[0].type();
  for (size_t i = 1; i < files.size(); ++i) {
    if (files[i].type() != type)
      return FileAttachment::Type::kUnknown;
  }
  return type;
}

TextAttachment::Type GetCommonTextAttachmentType(
    const std::vector<TextAttachment>& texts) {
  if (texts.empty())
    return TextAttachment::Type::kText;

  TextAttachment::Type type = texts[0].type();
  for (size_t i = 1; i < texts.size(); ++i) {
    if (texts[i].type() != type)
      return TextAttachment::Type::kText;
  }
  return type;
}

int GetFileAttachmentsCapitalizedStringId(
    const std::vector<FileAttachment>& files) {
  switch (GetCommonFileAttachmentType(files)) {
    case FileAttachment::Type::kApp:
      return IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_APPS;
    case FileAttachment::Type::kImage:
      return IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_IMAGES;
    case FileAttachment::Type::kUnknown:
      return IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_UNKNOWN;
    case FileAttachment::Type::kVideo:
      return IDS_NEARBY_FILE_ATTACHMENTS_CAPITALIZED_VIDEOS;
    default:
      return IDS_NEARBY_CAPITALIZED_UNKNOWN_ATTACHMENTS;
  }
}

int GetFileAttachmentsNotCapitalizedStringId(
    const std::vector<FileAttachment>& files) {
  switch (GetCommonFileAttachmentType(files)) {
    case FileAttachment::Type::kApp:
      return IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_APPS;
    case FileAttachment::Type::kImage:
      return IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_IMAGES;
    case FileAttachment::Type::kUnknown:
      return IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN;
    case FileAttachment::Type::kVideo:
      return IDS_NEARBY_FILE_ATTACHMENTS_NOT_CAPITALIZED_VIDEOS;
    default:
      return IDS_NEARBY_NOT_CAPITALIZED_UNKNOWN_ATTACHMENTS;
  }
}

int GetTextAttachmentsCapitalizedStringId(
    const std::vector<TextAttachment>& texts) {
  switch (GetCommonTextAttachmentType(texts)) {
    case TextAttachment::Type::kAddress:
      return IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_ADDRESSES;
    case TextAttachment::Type::kPhoneNumber:
      return IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_PHONE_NUMBERS;
    case TextAttachment::Type::kText:
      return IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_UNKNOWN;
    case TextAttachment::Type::kUrl:
      return IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_LINKS;
    default:
      return IDS_NEARBY_TEXT_ATTACHMENTS_CAPITALIZED_UNKNOWN;
  }
}

int GetTextAttachmentsNotCapitalizedStringId(
    const std::vector<TextAttachment>& texts) {
  switch (GetCommonTextAttachmentType(texts)) {
    case TextAttachment::Type::kAddress:
      return IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_ADDRESSES;
    case TextAttachment::Type::kPhoneNumber:
      return IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_PHONE_NUMBERS;
    case TextAttachment::Type::kText:
      return IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN;
    case TextAttachment::Type::kUrl:
      return IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_LINKS;
    default:
      return IDS_NEARBY_TEXT_ATTACHMENTS_NOT_CAPITALIZED_UNKNOWN;
  }
}

std::u16string GetAttachmentsString(const ShareTarget& share_target,
                                    bool use_capitalized_attachments) {
  size_t file_count = share_target.file_attachments.size();
  size_t text_count = share_target.text_attachments.size();
  int resource_id = use_capitalized_attachments
                        ? IDS_NEARBY_CAPITALIZED_UNKNOWN_ATTACHMENTS
                        : IDS_NEARBY_NOT_CAPITALIZED_UNKNOWN_ATTACHMENTS;

  if (file_count > 0 && text_count == 0)
    resource_id = use_capitalized_attachments
                      ? GetFileAttachmentsCapitalizedStringId(
                            share_target.file_attachments)
                      : GetFileAttachmentsNotCapitalizedStringId(
                            share_target.file_attachments);

  if (text_count > 0 && file_count == 0)
    resource_id = use_capitalized_attachments
                      ? GetTextAttachmentsCapitalizedStringId(
                            share_target.text_attachments)
                      : GetTextAttachmentsNotCapitalizedStringId(
                            share_target.text_attachments);

  return l10n_util::GetPluralStringFUTF16(resource_id, text_count + file_count);
}

std::u16string FormatNotificationTitle(const ShareTarget& share_target,
                                       int resource_id,
                                       bool use_capitalized_attachments) {
  std::u16string attachments =
      GetAttachmentsString(share_target, use_capitalized_attachments);

  std::u16string device_name = base::UTF8ToUTF16(share_target.device_name);
  size_t attachment_count = share_target.file_attachments.size() +
                            share_target.text_attachments.size();

  if (!share_target.wifi_credentials_attachments.empty()) {
    std::u16string network_name =
        base::UTF8ToUTF16(share_target.wifi_credentials_attachments[0].ssid());
    return base::ReplaceStringPlaceholders(
        l10n_util::GetStringUTF16(resource_id), {network_name, device_name},
        /*offsets=*/nullptr);
  } else {
    return base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(resource_id, attachment_count),
        {attachments, device_name}, /*offsets=*/nullptr);
  }
}

std::u16string GetProgressNotificationTitle(const ShareTarget& share_target) {
  if (!share_target.wifi_credentials_attachments.empty()) {
    return FormatNotificationTitle(
        share_target,
        IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE_WIFI_CREDENTIALS,
        /*use_capitalized_attachments=*/false);
  } else {
    return FormatNotificationTitle(
        share_target,
        share_target.is_incoming
            ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
            : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE,
        /*use_capitalized_attachments=*/false);
  }
}

std::u16string GetSuccessNotificationTitle(const ShareTarget& share_target) {
  if (!share_target.wifi_credentials_attachments.empty()) {
    return FormatNotificationTitle(
        share_target,
        IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE_WIFI_CREDENTIALS,
        /*use_capitalized_attachments=*/false);
  } else {
    return FormatNotificationTitle(
        share_target,
        share_target.is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE
                                 : IDS_NEARBY_NOTIFICATION_SEND_SUCCESS_TITLE,
        /*use_capitalized_attachments=*/true);
  }
}

std::u16string GetFailureNotificationTitle(const ShareTarget& share_target) {
  if (!share_target.wifi_credentials_attachments.empty()) {
    return FormatNotificationTitle(
        share_target,
        IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE_WIFI_CREDENTIALS,
        /*use_capitalized_attachments=*/false);
  } else {
    return FormatNotificationTitle(
        share_target,
        share_target.is_incoming ? IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE
                                 : IDS_NEARBY_NOTIFICATION_SEND_FAILURE_TITLE,
        /*use_capitalized_attachments=*/false);
  }
}

std::optional<std::u16string> GetFailureNotificationMessage(
    TransferMetadata::Status status) {
  switch (status) {
    case TransferMetadata::Status::kTimedOut:
      return l10n_util::GetStringUTF16(IDS_NEARBY_ERROR_TIME_OUT);
    case TransferMetadata::Status::kNotEnoughSpace:
      return l10n_util::GetStringUTF16(IDS_NEARBY_ERROR_NOT_ENOUGH_SPACE);
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return l10n_util::GetStringUTF16(IDS_NEARBY_ERROR_UNSUPPORTED_FILE_TYPE);
    default:
      return std::nullopt;
  }
}

std::u16string GetConnectionRequestNotificationMessage(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  std::u16string attachments =
      GetAttachmentsString(share_target, /*use_capitalized_attachments=*/false);
  std::u16string device_name = base::UTF8ToUTF16(share_target.device_name);

  size_t attachment_count = share_target.file_attachments.size() +
                            share_target.text_attachments.size();
  std::u16string message;
  if (!share_target.wifi_credentials_attachments.empty()) {
    message = base::ReplaceStringPlaceholders(
        l10n_util::GetStringUTF16(
            IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE_WIFI_CREDENTIALS),
        device_name, /*offsets=*/nullptr);
  } else {
    message = base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(
            IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE,
            attachment_count),
        {device_name, attachments}, /*offsets=*/nullptr);
  }

  if (transfer_metadata.token()) {
    std::u16string token = l10n_util::GetStringFUTF16(
        IDS_NEARBY_SECURE_CONNECTION_ID,
        base::UTF8ToUTF16(*transfer_metadata.token()));
    message = base::StrCat({message, u"\n", token});
  }

  return message;
}

std::optional<std::u16string> GetReceivedNotificationTextMessage(
    const ShareTarget& share_target) {
  size_t text_count = share_target.text_attachments.size();
  if (text_count < 1) {
    return std::nullopt;
  }

  const TextAttachment& attachment = share_target.text_attachments[0];
  return base::UTF8ToUTF16(attachment.GetDescription());
}

ui::ImageModel GetImageFromShareTarget(const ShareTarget& share_target) {
  // TODO(crbug.com/40138752): Create or get profile picture of |share_target|.
  return ui::ImageModel();
}

NearbyNotificationManager::ReceivedContentType GetReceivedContentType(
    const ShareTarget& share_target) {
  if (!share_target.wifi_credentials_attachments.empty()) {
    return NearbyNotificationManager::ReceivedContentType::kWifiCredentials;
  }

  if (!share_target.text_attachments.empty()) {
    const TextAttachment& file = share_target.text_attachments[0];
    if (share_target.text_attachments.size() == 1 &&
        file.type() == sharing::mojom::TextMetadata::Type::kUrl) {
      return NearbyNotificationManager::ReceivedContentType::kSingleUrl;
    }

    return NearbyNotificationManager::ReceivedContentType::kText;
  }

  if (share_target.file_attachments.size() != 1)
    return NearbyNotificationManager::ReceivedContentType::kFiles;

  const FileAttachment& file = share_target.file_attachments[0];
  if (file.type() == sharing::mojom::FileMetadata::Type::kImage)
    return NearbyNotificationManager::ReceivedContentType::kSingleImage;

  return NearbyNotificationManager::ReceivedContentType::kFiles;
}

class ProgressNotificationDelegate : public NearbyNotificationDelegate {
 public:
  explicit ProgressNotificationDelegate(NearbyNotificationManager* manager,
                                        bool awaiting_remote_acceptance)
      : manager_(manager),
        awaiting_remote_acceptance_(awaiting_remote_acceptance) {}
  ~ProgressNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const std::optional<int>& action_index) override {
    // Clicking on the notification is a noop.
    if (!action_index)
      return;
    // Clicking on the only (cancel) button cancels the transfer.
    DCHECK_EQ(0, *action_index);

    // In the receiving case, the progress notification is showed after the
    // transfer is accepted, but before the |TransferMetadata::Status| is
    // actually |kInProgress|. In this case, it is more appropriate to reject
    // the transfer, but as far as the user is concerned, it looks like a
    // cancellation.
    if (awaiting_remote_acceptance_)
      manager_->RejectTransfer();
    else
      manager_->CancelTransfer();
  }

  void OnClose(const std::string& notification_id) override {
    if (awaiting_remote_acceptance_)
      manager_->RejectTransfer();
    else
      manager_->CancelTransfer();
  }

 private:
  raw_ptr<NearbyNotificationManager> manager_;
  bool awaiting_remote_acceptance_ = false;
};

class ConnectionRequestNotificationDelegate
    : public NearbyNotificationDelegate {
 public:
  explicit ConnectionRequestNotificationDelegate(
      NearbyNotificationManager* manager)
      : manager_(manager) {}
  ~ConnectionRequestNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const std::optional<int>& action_index) override {
    // Clicking on the notification is a noop.
    if (!action_index)
      return;

    switch (*action_index) {
      case 0:
        manager_->AcceptTransfer();
        break;
      case 1:
        manager_->RejectTransfer();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void OnClose(const std::string& notification_id) override {
    manager_->RejectTransfer();
  }

 private:
  raw_ptr<NearbyNotificationManager> manager_;
};

class ReceivedImageDecoder : public ImageDecoder::ImageRequest {
 public:
  using ImageCallback = base::OnceCallback<void(const SkBitmap& decoded_image)>;

  explicit ReceivedImageDecoder(ImageCallback callback)
      : callback_(std::move(callback)) {}
  ~ReceivedImageDecoder() override = default;

  void DecodeImage(const std::optional<base::FilePath>& image_path) {
    if (!image_path) {
      OnDecodeImageFailed();
      return;
    }

    auto contents = std::make_unique<std::string>();
    auto* contents_ptr = contents.get();

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::ReadFileToString, *image_path, contents_ptr),
        base::BindOnce(&ReceivedImageDecoder::OnFileRead,
                       weak_ptr_factory_.GetWeakPtr(), std::move(contents)));
  }

  // ImageDecoder::ImageRequest implementation:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    std::move(callback_).Run(decoded_image);
    delete this;
  }

  void OnDecodeImageFailed() override {
    std::move(callback_).Run(SkBitmap());
    delete this;
  }

 private:
  void OnFileRead(std::unique_ptr<std::string> contents,
                  bool is_contents_read) {
    if (!is_contents_read || !contents || contents->empty()) {
      CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Image contents not found.";
      OnDecodeImageFailed();
      return;
    }

    ImageDecoder::Start(this, std::move(*contents));
  }

  ImageCallback callback_;
  base::WeakPtrFactory<ReceivedImageDecoder> weak_ptr_factory_{this};
};

class SuccessNotificationDelegate : public NearbyNotificationDelegate {
 public:
  SuccessNotificationDelegate(
      NearbyNotificationManager* manager,
      Profile* profile,
      ShareTarget share_target,
      NearbyNotificationManager::ReceivedContentType type,
      const SkBitmap& image,
      base::OnceCallback<
          void(NearbyNotificationManager::SuccessNotificationAction)>
          testing_callback)
      : manager_(manager),
        profile_(profile),
        share_target_(std::move(share_target)),
        type_(type),
        image_(image),
        testing_callback_(std::move(testing_callback)) {}
  ~SuccessNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const std::optional<int>& action_index) override {
    switch (type_) {
      case NearbyNotificationManager::ReceivedContentType::kText:
        if (action_index.has_value() && action_index.value() == 0) {
          // Don't overwrite clipboard if user clicks notification body
          CopyTextToClipboard();
        }
        break;
      case NearbyNotificationManager::ReceivedContentType::kSingleUrl:
        if (!action_index.has_value()) {
          OpenTextLink();
          break;
        }
        switch (*action_index) {
          case 0:
            OpenTextLink();
            break;
          case 1:
            CopyTextToClipboard();
            break;
          default:
            NOTREACHED_IN_MIGRATION();
            break;
        }
        break;
      case NearbyNotificationManager::ReceivedContentType::kSingleImage:
        if (!action_index.has_value()) {
          OpenDownloadsFolder();
          break;
        }
        switch (*action_index) {
          case 0:
            OpenDownloadsFolder();
            break;
          case 1:
            CopyImageToClipboard();
            break;
          default:
            NOTREACHED_IN_MIGRATION();
            break;
        }
        break;
      case NearbyNotificationManager::ReceivedContentType::kFiles:
        OpenDownloadsFolder();
        break;
      case NearbyNotificationManager::ReceivedContentType::kWifiCredentials:
        OpenWifiNetworksList();
        break;
    }

    manager_->CloseSuccessNotification(notification_id);
  }

  void OnClose(const std::string& notification_id) override {
    manager_->CloseSuccessNotification(notification_id);
  }

 private:
  void OpenDownloadsFolder() {
    platform_util::OpenItem(
        profile_,
        DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
            ->DownloadPath(),
        platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(NearbyNotificationManager::SuccessNotificationAction::
                   kOpenDownloads);
    }
  }

  void OpenTextLink() {
    const std::string& url = share_target_.text_attachments[0].text_body();
    manager_->OpenURL(GURL(url));

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(NearbyNotificationManager::SuccessNotificationAction::kOpenUrl);
    }
  }

  void CopyTextToClipboard() {
    DCHECK_GT(share_target_.text_attachments.size(), 0u);
    const std::string& text = share_target_.text_attachments[0].text_body();
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(NearbyNotificationManager::SuccessNotificationAction::kCopyText);
    }
  }

  void CopyImageToClipboard() {
    DCHECK(!image_.isNull());
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteImage(image_);

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(
              NearbyNotificationManager::SuccessNotificationAction::kCopyImage);
    }
  }

  void OpenWifiNetworksList() {
    manager_->OpenWifiNetworksList();

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(NearbyNotificationManager::SuccessNotificationAction::
                   kOpenWifiNetworksList);
    }
  }

  raw_ptr<NearbyNotificationManager> manager_;
  raw_ptr<Profile> profile_;
  ShareTarget share_target_;
  NearbyNotificationManager::ReceivedContentType type_;
  SkBitmap image_;
  base::OnceCallback<void(NearbyNotificationManager::SuccessNotificationAction)>
      testing_callback_;
};

class NearbyDeviceTryingToShareNotificationDelegate
    : public NearbyNotificationDelegate {
 public:
  explicit NearbyDeviceTryingToShareNotificationDelegate(
      NearbyNotificationManager* manager)
      : manager_(manager) {}
  ~NearbyDeviceTryingToShareNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const std::optional<int>& action_index) override {
    if (!action_index) {
      return;
    }

    switch (*action_index) {
      case 0:
        manager_->OnNearbyDeviceTryingToShareClicked();
        break;
      case 1:
        manager_->OnNearbyDeviceTryingToShareDismissed(
            /*did_click_dismiss=*/true);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void OnClose(const std::string& notification_id) override {
    manager_->OnNearbyDeviceTryingToShareDismissed(/*did_click_dismiss=*/false);
  }

 private:
  raw_ptr<NearbyNotificationManager> manager_;
};

class NearbyVisibilityReminderNotificationDelegate
    : public NearbyNotificationDelegate {
 public:
  explicit NearbyVisibilityReminderNotificationDelegate(
      NearbyNotificationManager* manager)
      : manager_(manager) {}

  ~NearbyVisibilityReminderNotificationDelegate() override = default;

  void OnClick(const std::string& notification_id,
               const std::optional<int>& action_index) override {
    if (!action_index) {
      // Open settings when user click the notification.
      manager_->OnNearbyVisibilityReminderClicked();
      return;
    }

    switch (*action_index) {
      case 0:
        manager_->OnNearbyVisibilityReminderClicked();
        break;
      case 1:
        manager_->OnNearbyVisibilityReminderDismissed();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void OnClose(const std::string& notification_id) override {
    manager_->OnNearbyVisibilityReminderDismissed();
  }

 private:
  raw_ptr<NearbyNotificationManager> manager_;
};

bool ShouldShowNearbyDeviceTryingToShareNotification(
    PrefService* pref_service) {
  base::Time last_dismissed = pref_service->GetTime(
      prefs::kNearbySharingNearbyDeviceTryingToShareDismissedTimePrefName);
  if (last_dismissed.is_null())
    return true;

  base::TimeDelta last_dismissed_delta = base::Time::Now() - last_dismissed;
  if (last_dismissed_delta <
      NearbyNotificationManager::kNearbyDeviceTryingToShareDismissedTimeout) {
    CD_LOG(VERBOSE, Feature::NS)
        << "Not showing onboarding notification: the user recently "
           "dismissed the notification.";
    return false;
  }

  return true;
}

bool ShouldShowNearbyVisibilityReminderNotification(PrefService* pref_service) {
  nearby_share::mojom::Visibility visibility =
      static_cast<nearby_share::mojom::Visibility>(pref_service->GetInteger(
          prefs::kNearbySharingBackgroundVisibilityName));

  return visibility == nearby_share::mojom::Visibility::kAllContacts ||
         visibility == nearby_share::mojom::Visibility::kSelectedContacts;
}

void UpdateNearbyDeviceTryingToShareDismissedTime(PrefService* pref_service) {
  pref_service->SetTime(
      prefs::kNearbySharingNearbyDeviceTryingToShareDismissedTimePrefName,
      base::Time::Now());
}

bool ShouldClearNotification(
    std::optional<TransferMetadata::Status> last_status,
    TransferMetadata::Status new_status) {
  if (!last_status)
    return true;

  // While receiving and waiting for the sender to accept, we are showing a
  // progress notification with 0% progress. We need not close the
  // progress notification when we move to showing determinate progress.
  if (*last_status == TransferMetadata::Status::kAwaitingRemoteAcceptance &&
      new_status == TransferMetadata::Status::kInProgress)
    return false;

  // In all other cases, if the status has changed, the previous notification
  // should be cleared.
  return *last_status != new_status;
}

}  // namespace

// static
constexpr base::TimeDelta
    NearbyNotificationManager::kNearbyDeviceTryingToShareDismissedTimeout;

void NearbyNotificationManager::SettingsOpener::ShowSettingsPage(
    Profile* profile,
    const std::string& sub_page) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                               sub_page);
}

NearbyNotificationManager::NearbyNotificationManager(
    NotificationDisplayService* notification_display_service,
    NearbySharingService* nearby_service,
    PrefService* pref_service,
    Profile* profile)
    : notification_display_service_(notification_display_service),
      nearby_service_(nearby_service),
      pref_service_(pref_service),
      profile_(profile),
      settings_opener_(std::make_unique<SettingsOpener>()) {
  DCHECK(notification_display_service_);
  DCHECK(nearby_service_);
  DCHECK(pref_service_);
  nearby_service_->AddObserver(this);
  nearby_service_->RegisterReceiveSurface(
      this, NearbySharingService::ReceiveSurfaceState::kBackground);
  nearby_service_->RegisterSendSurface(
      this, this, NearbySharingService::SendSurfaceState::kBackground);
}

NearbyNotificationManager::~NearbyNotificationManager() {
  nearby_service_->RemoveObserver(this);
  nearby_service_->UnregisterReceiveSurface(this);
  nearby_service_->UnregisterSendSurface(this, this);
}

void NearbyNotificationManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  if (!share_target_)
    share_target_ = share_target;
  DCHECK_EQ(share_target_->id, share_target.id);

  if (ShouldClearNotification(last_transfer_status_,
                              transfer_metadata.status())) {
    // Close any previous notifications, to allow subsequent high-priority
    // notifications to pop up.
    CloseTransfer();
  }
  last_transfer_status_ = transfer_metadata.status();

  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kInProgress:
      ShowProgress(share_target, transfer_metadata);
      break;
    case TransferMetadata::Status::kCancelled:
      // Only show the notification if the remote cancelled.
      if (!nearby_service_->DidLocalUserCancelTransfer(share_target))
        ShowCancelled(share_target);
      break;
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
    case TransferMetadata::Status::kExternalProviderLaunched:
      // Any previous notifications have been closed with the status change
      // check above that called CloseTransfer(). No notification is currently
      // shown for these statuses, so break.
      break;
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      // Only incoming transfers are handled via notifications.
      if (share_target.is_incoming)
        // Show a progress notification with 0% progress while
        // waiting for the sender to accept.
        ShowProgress(share_target, transfer_metadata);
      break;
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
      // Only incoming transfers are handled via notifications.
      // Don't show notification for auto-accept self shares.
      if (share_target.is_incoming && !share_target.CanAutoAccept()) {
        ShowConnectionRequest(share_target, transfer_metadata);
      }
      break;
    case TransferMetadata::Status::kComplete:
      ShowSuccess(share_target);
      break;
    case TransferMetadata::Status::kRejected:
    case TransferMetadata::Status::kTimedOut:
    case TransferMetadata::Status::kFailed:
    case TransferMetadata::Status::kNotEnoughSpace:
    case TransferMetadata::Status::kUnsupportedAttachmentType:
    case TransferMetadata::Status::kDecodeAdvertisementFailed:
    case TransferMetadata::Status::kMissingTransferUpdateCallback:
    case TransferMetadata::Status::kMissingShareTarget:
    case TransferMetadata::Status::kMissingEndpointId:
    case TransferMetadata::Status::kMissingPayloads:
    case TransferMetadata::Status::kPairedKeyVerificationFailed:
    case TransferMetadata::Status::kInvalidIntroductionFrame:
    case TransferMetadata::Status::kIncompletePayloads:
    case TransferMetadata::Status::kFailedToCreateShareTarget:
    case TransferMetadata::Status::kFailedToInitiateOutgoingConnection:
    case TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse:
    case TransferMetadata::Status::kUnexpectedDisconnection:
      ShowFailure(share_target, transfer_metadata);
      break;
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kMediaUnavailable:
    case TransferMetadata::Status::kMediaDownloading:
      // Ignore
      break;
  }

  if (transfer_metadata.is_final_status()) {
    share_target_.reset();
    last_transfer_status_.reset();
  }
}

void NearbyNotificationManager::OnShareTargetDiscovered(
    ShareTarget share_target) {
  // Nothing to do here.
}

void NearbyNotificationManager::OnShareTargetLost(ShareTarget share_target) {
  // Nothing to do here.
}

void NearbyNotificationManager::OnNearbyProcessStopped() {
  if (share_target_ && last_transfer_status_) {
    CloseTransfer();
    ShowFailure(
        *share_target_,
        TransferMetadataBuilder().set_status(*last_transfer_status_).build());
  }
  share_target_ = std::nullopt;
  last_transfer_status_ = std::nullopt;
}

void NearbyNotificationManager::OnFastInitiationDevicesDetected() {
  ShowNearbyDeviceTryingToShare();
}

void NearbyNotificationManager::OnFastInitiationDevicesNotDetected() {
  CloseNearbyDeviceTryingToShare();
}

void NearbyNotificationManager::OnFastInitiationScanningStopped() {
  CloseNearbyDeviceTryingToShare();
}

void NearbyNotificationManager::ShowProgress(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyInProgressNotificationId);
  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification.set_title(GetProgressNotificationTitle(share_target));
  notification.set_never_timeout(true);
  notification.set_pinned(true);
  notification.set_priority(message_center::NotificationPriority::MAX_PRIORITY);

  // Show 0% progress while waiting for remote device to accept.
  if (transfer_metadata.status() == TransferMetadata::Status::kInProgress)
    notification.set_progress(transfer_metadata.progress());
  else
    notification.set_progress(0);

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  notification.set_buttons(notification_actions);

  delegate_map_[notification.id()] =
      std::make_unique<ProgressNotificationDelegate>(
          this, /*awaiting_remote_acceptance=*/transfer_metadata.status() ==
                    TransferMetadata::Status::kAwaitingRemoteAcceptance);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowConnectionRequest(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyInProgressNotificationId);
  notification.set_title(
      features::IsNameEnabled()
          ? NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE_PH)
          : l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE));
  notification.set_message(
      GetConnectionRequestNotificationMessage(share_target, transfer_metadata));
  notification.set_icon(GetImageFromShareTarget(share_target));
  notification.set_never_timeout(true);
  notification.set_priority(message_center::NotificationPriority::MAX_PRIORITY);

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACCEPT_ACTION));
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION));
  notification.set_buttons(notification_actions);

  delegate_map_[notification.id()] =
      std::make_unique<ConnectionRequestNotificationDelegate>(this);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowNearbyDeviceTryingToShare() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ShouldShowNearbyDeviceTryingToShareNotification(pref_service_))
    return;

  CD_LOG(INFO, Feature::NS) << "Showing fast initiation notification.";

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyDeviceTryingToShareNotificationId);

  bool is_onboarding_complete = pref_service_->GetBoolean(
      prefs::kNearbySharingOnboardingCompletePrefName);

  notification.set_title(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE));

  const std::u16string onboarding_message =
      features::IsNameEnabled()
          ? NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE_PH)
          : l10n_util::GetStringUTF16(
                IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE);

  notification.set_message(is_onboarding_complete
                               ? l10n_util::GetStringUTF16(
                                     IDS_NEARBY_NOTIFICATION_GO_VISIBLE_MESSAGE)
                               : onboarding_message);

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(l10n_util::GetStringUTF16(
      is_onboarding_complete ? IDS_NEARBY_NOTIFICATION_GO_VISIBLE_ACTION
                             : IDS_NEARBY_NOTIFICATION_SET_UP_ACTION));
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));
  notification.set_buttons(notification_actions);

  delegate_map_[kNearbyDeviceTryingToShareNotificationId] =
      std::make_unique<NearbyDeviceTryingToShareNotificationDelegate>(this);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);

  last_device_nearby_sharing_notification_shown_timestamp_ =
      base::TimeTicks::Now();

  if (is_onboarding_complete) {
    RecordNearbyShareDeviceNearbySharingNotificationFlowEvent(
        NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent::
            kNotificationShown);
  } else {
    RecordNearbyShareSetupNotificationFlowEvent(
        NearbyShareBackgroundScanningSetupNotificationFlowEvent::
            kNotificationShown);
  }
}

void NearbyNotificationManager::ShowSuccess(const ShareTarget& share_target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!share_target.is_incoming) {
    std::string notification_id =
        CreateNotificationIdForShareTarget(share_target);
    message_center::Notification notification =
        CreateNearbyNotification(notification_id);
    notification.set_title(GetSuccessNotificationTitle(share_target));

    delegate_map_.erase(notification_id);

    notification_display_service_->Display(
        NotificationHandler::Type::NEARBY_SHARE, notification,
        /*metadata=*/nullptr);
    return;
  }

  ReceivedContentType type = GetReceivedContentType(share_target);

  if (type != ReceivedContentType::kSingleImage) {
    ShowIncomingSuccess(share_target, type, /*image=*/SkBitmap());
    return;
  }

  // ReceivedContentType::kSingleImage means exactly one image file.
  DCHECK_EQ(1u, share_target.file_attachments.size());

  // ReceivedImageDecoder will delete itself.
  auto* image_decoder = new ReceivedImageDecoder(
      base::BindOnce(&NearbyNotificationManager::ShowIncomingSuccess,
                     weak_ptr_factory_.GetWeakPtr(), share_target, type));
  image_decoder->DecodeImage(share_target.file_attachments[0].file_path());
}

void NearbyNotificationManager::ShowIncomingSuccess(
    const ShareTarget& share_target,
    ReceivedContentType type,
    const SkBitmap& image) {
  std::string notification_id =
      CreateNotificationIdForShareTarget(share_target);
  message_center::Notification notification =
      CreateNearbyNotification(notification_id);
  notification.set_title(GetSuccessNotificationTitle(share_target));

  // Revert to generic file handling if image decoding failed.
  if (type == ReceivedContentType::kSingleImage && image.isNull())
    type = ReceivedContentType::kFiles;

  if (!image.isNull()) {
    notification.set_type(message_center::NOTIFICATION_TYPE_IMAGE);
    notification.SetImage(gfx::Image::CreateFrom1xBitmap(image));
  }

  std::vector<message_center::ButtonInfo> notification_actions;
  switch (type) {
    case ReceivedContentType::kText: {
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD));
      std::optional<std::u16string> message =
          GetReceivedNotificationTextMessage(share_target);
      if (message) {
        notification.set_message(message.value());
      }
      break;
    }
    case ReceivedContentType::kSingleUrl:
      notification_actions.emplace_back(
          l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ACTION_OPEN_URL));
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD));
      break;
    case ReceivedContentType::kSingleImage:
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER));
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_COPY_TO_CLIPBOARD));
      break;
    case ReceivedContentType::kFiles:
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_OPEN_FOLDER));
      break;
    case ReceivedContentType::kWifiCredentials:
      notification_actions.emplace_back(l10n_util::GetStringUTF16(
          IDS_NEARBY_NOTIFICATION_ACTION_OPEN_NETWORK_LIST));
      break;
  }
  notification.set_buttons(notification_actions);

  delegate_map_[notification_id] =
      std::make_unique<SuccessNotificationDelegate>(
          this, profile_, share_target, type, image,
          std::move(success_action_test_callback_));

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);

  ash::HoldingSpaceKeyedService* holding_space_keyed_service =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile_);
  if (holding_space_keyed_service) {
    for (const auto& file : share_target.file_attachments) {
      if (file.file_path().has_value())
        holding_space_keyed_service->AddItemOfType(
            ash::HoldingSpaceItem::Type::kNearbyShare,
            file.file_path().value());
    }
  }
}

void NearbyNotificationManager::ShowFailure(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string notification_id =
      CreateNotificationIdForShareTarget(share_target);
  message_center::Notification notification =
      CreateNearbyNotification(notification_id);
  notification.set_title(GetFailureNotificationTitle(share_target));

  std::optional<std::u16string> message =
      GetFailureNotificationMessage(transfer_metadata.status());
  if (message) {
    notification.set_message(*message);
  }

  delegate_map_.erase(notification_id);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowCancelled(const ShareTarget& share_target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string notification_id =
      CreateNotificationIdForShareTarget(share_target);
  message_center::Notification notification =
      CreateNearbyNotification(notification_id);

  notification.set_title(base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SENDER_CANCELLED),
      {base::UTF8ToUTF16(share_target.device_name)}, /*offsets=*/nullptr));

  delegate_map_.erase(notification_id);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowVisibilityReminder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ShouldShowNearbyVisibilityReminderNotification(pref_service_)) {
    return;
  }

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyVisibilityReminderNotificationId);
  notification.set_title(l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_TITLE));
  notification.set_message(l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_VISIBILITY_REMINDER_MESSAGE));

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_GO_TO_SETTINGS_ACTION));
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DISMISS_ACTION));

  notification.set_buttons(notification_actions);

  delegate_map_[kNearbyVisibilityReminderNotificationId] =
      std::make_unique<NearbyVisibilityReminderNotificationDelegate>(this);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::CloseTransfer() {
  delegate_map_.erase(kNearbyInProgressNotificationId);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       kNearbyInProgressNotificationId);
}

void NearbyNotificationManager::CloseNearbyDeviceTryingToShare() {
  delegate_map_.erase(kNearbyDeviceTryingToShareNotificationId);
  notification_display_service_->Close(
      NotificationHandler::Type::NEARBY_SHARE,
      kNearbyDeviceTryingToShareNotificationId);
}

void NearbyNotificationManager::CloseVisibilityReminder() {
  delegate_map_.erase(kNearbyVisibilityReminderNotificationId);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       kNearbyVisibilityReminderNotificationId);
}

NearbyNotificationDelegate* NearbyNotificationManager::GetNotificationDelegate(
    const std::string& notification_id) {
  auto iter = delegate_map_.find(notification_id);
  if (iter == delegate_map_.end())
    return nullptr;

  return iter->second.get();
}

void NearbyNotificationManager::OpenURL(GURL url) {
  nearby_service_->OpenURL(url);
}

void NearbyNotificationManager::OpenWifiNetworksList() {
  settings_opener_->ShowSettingsPage(
      profile_, chromeos::settings::mojom::kKnownNetworksSubpagePath);
}

void NearbyNotificationManager::CancelTransfer() {
  CloseTransfer();
  nearby_service_->Cancel(*share_target_, base::DoNothing());
}

void NearbyNotificationManager::RejectTransfer() {
  CloseTransfer();
  nearby_service_->Reject(*share_target_, base::DoNothing());
}

void NearbyNotificationManager::AcceptTransfer() {
  // Do not close the notification as it will be replaced soon.
  nearby_service_->Accept(*share_target_, base::DoNothing());
}

void NearbyNotificationManager::OnNearbyDeviceTryingToShareClicked() {
  CloseNearbyDeviceTryingToShare();

  std::string path =
      std::string(chromeos::settings::mojom::kNearbyShareSubpagePath) +
      "?receive&entrypoint=notification&";

  // Append a timestamp to ensure that the URL that we use to open the receive
  // dialog on the Nearby Share subpage is unique each time the user clicks on
  // an instance of this notification. This ensures that the page will reload
  // and we will restart high visibility mode. Since we don't actually read this
  // timestamp, it will not cause a problem if the user manually reloads the
  // page with a stale timestamp.
  path += "&time=" + GetTimestampString();
  settings_opener_->ShowSettingsPage(profile_, path);

  bool is_onboarding_complete = pref_service_->GetBoolean(
      prefs::kNearbySharingOnboardingCompletePrefName);
  if (is_onboarding_complete) {
    RecordNearbyShareDeviceNearbySharingNotificationFlowEvent(
        NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent::
            kEnable);
    RecordNearbyShareDeviceNearbySharingNotificationTimeToAction(
        base::TimeTicks::Now() -
        last_device_nearby_sharing_notification_shown_timestamp_);
  } else {
    RecordNearbyShareSetupNotificationFlowEvent(
        NearbyShareBackgroundScanningSetupNotificationFlowEvent::kSetup);
    RecordNearbyShareSetupNotificationTimeToAction(
        base::TimeTicks::Now() -
        last_device_nearby_sharing_notification_shown_timestamp_);
  }
}

void NearbyNotificationManager::OnNearbyDeviceTryingToShareDismissed(
    bool did_click_dismiss) {
  CloseNearbyDeviceTryingToShare();
  UpdateNearbyDeviceTryingToShareDismissedTime(pref_service_);

  bool is_onboarding_complete = pref_service_->GetBoolean(
      prefs::kNearbySharingOnboardingCompletePrefName);

  if (is_onboarding_complete) {
    RecordNearbyShareDeviceNearbySharingNotificationFlowEvent(
        did_click_dismiss
            ? NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent::
                  kDismiss
            : NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent::
                  kExit);
  } else {
    RecordNearbyShareSetupNotificationFlowEvent(
        did_click_dismiss
            ? NearbyShareBackgroundScanningSetupNotificationFlowEvent::kDismiss
            : NearbyShareBackgroundScanningSetupNotificationFlowEvent::kExit);
    RecordNearbyShareSetupNotificationTimeToAction(
        base::TimeTicks::Now() -
        last_device_nearby_sharing_notification_shown_timestamp_);
  }
}

void NearbyNotificationManager::CloseSuccessNotification(
    const std::string& notification_id) {
  delegate_map_.erase(notification_id);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       notification_id);
}

void NearbyNotificationManager::OnNearbyVisibilityReminderClicked() {
  CloseVisibilityReminder();

  std::string path =
      std::string(chromeos::settings::mojom::kNearbyShareSubpagePath) +
      "?visibility";

  settings_opener_->ShowSettingsPage(profile_, path);
}

void NearbyNotificationManager::OnNearbyVisibilityReminderDismissed() {
  CloseVisibilityReminder();
}

void NearbyNotificationManager::SetOnSuccessClickedForTesting(
    base::OnceCallback<void(SuccessNotificationAction)> callback) {
  success_action_test_callback_ = std::move(callback);
}

void NearbyNotificationManager::SetSettingsOpenerForTesting(
    std::unique_ptr<SettingsOpener> settings_opener) {
  settings_opener_ = std::move(settings_opener);
}
