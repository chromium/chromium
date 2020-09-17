// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

constexpr char kNearbyNotificationId[] = "chrome://nearby";
constexpr char kNearbyOnboardingNotificationId[] = "chrome://nearby/onboarding";
constexpr char kNearbyNotifier[] = "nearby";

// Creates a default Nearby Share notification with empty content.
message_center::Notification CreateNearbyNotification(const std::string& id) {
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      /*title=*/base::string16(),
      /*message=*/base::string16(),
      /*icon=*/gfx::Image(),
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNearbyNotifier),
      /*optional_fields=*/{},
      /*delegate=*/nullptr);
  notification.set_vector_small_image(kNearbyShareIcon);

  // TODO(crbug.com/1102348): Also show settings for other platforms once there
  // is a nearby settings page in Chrome browser.
#if defined(OS_CHROMEOS)
  notification.set_settings_button_handler(
      message_center::SettingsButtonHandler::DELEGATE);
#endif

  return notification;
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

int GetFileAttachmentsStringId(const std::vector<FileAttachment>& files) {
  switch (GetCommonFileAttachmentType(files)) {
    case FileAttachment::Type::kApp:
      return IDS_NEARBY_FILE_ATTACHMENTS_APPS;
    case FileAttachment::Type::kImage:
      return IDS_NEARBY_FILE_ATTACHMENTS_IMAGES;
    case FileAttachment::Type::kUnknown:
      return IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN;
    case FileAttachment::Type::kVideo:
      return IDS_NEARBY_FILE_ATTACHMENTS_VIDEOS;
    default:
      return IDS_NEARBY_UNKNOWN_ATTACHMENTS;
  }
}

int GetTextAttachmentsStringId(const std::vector<TextAttachment>& texts) {
  switch (GetCommonTextAttachmentType(texts)) {
    case TextAttachment::Type::kAddress:
      return IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES;
    case TextAttachment::Type::kPhoneNumber:
      return IDS_NEARBY_TEXT_ATTACHMENTS_PHONE_NUMBERS;
    case TextAttachment::Type::kText:
      return IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN;
    case TextAttachment::Type::kUrl:
      return IDS_NEARBY_TEXT_ATTACHMENTS_LINKS;
    default:
      return IDS_NEARBY_UNKNOWN_ATTACHMENTS;
  }
}

base::string16 GetAttachmentsString(const ShareTarget& share_target) {
  size_t file_count = share_target.file_attachments.size();
  size_t text_count = share_target.text_attachments.size();
  int resource_id = IDS_NEARBY_UNKNOWN_ATTACHMENTS;

  if (file_count > 0 && text_count == 0)
    resource_id = GetFileAttachmentsStringId(share_target.file_attachments);

  if (text_count > 0 && file_count == 0)
    resource_id = GetTextAttachmentsStringId(share_target.text_attachments);

  return l10n_util::GetPluralStringFUTF16(resource_id, text_count + file_count);
}

base::string16 FormatNotificationTitle(const ShareTarget& share_target,
                                       int resource_id) {
  base::string16 attachments = GetAttachmentsString(share_target);
  base::string16 device_name = base::ASCIIToUTF16(share_target.device_name);
  size_t attachment_count = share_target.file_attachments.size() +
                            share_target.text_attachments.size();

  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(resource_id, attachment_count),
      {attachments, device_name}, /*offsets=*/nullptr);
}

base::string16 GetProgressNotificationTitle(const ShareTarget& share_target) {
  return FormatNotificationTitle(
      share_target, share_target.is_incoming
                        ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
                        : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE);
}

base::string16 GetSuccessNotificationTitle(const ShareTarget& share_target) {
  return FormatNotificationTitle(
      share_target, share_target.is_incoming
                        ? IDS_NEARBY_NOTIFICATION_RECEIVE_SUCCESS_TITLE
                        : IDS_NEARBY_NOTIFICATION_SEND_SUCCESS_TITLE);
}

base::string16 GetFailureNotificationTitle(const ShareTarget& share_target) {
  return FormatNotificationTitle(
      share_target, share_target.is_incoming
                        ? IDS_NEARBY_NOTIFICATION_RECEIVE_FAILURE_TITLE
                        : IDS_NEARBY_NOTIFICATION_SEND_FAILURE_TITLE);
}

base::string16 GetConnectionRequestNotificationMessage(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  base::string16 attachments = GetAttachmentsString(share_target);
  base::string16 device_name = base::ASCIIToUTF16(share_target.device_name);

  size_t attachment_count = share_target.file_attachments.size() +
                            share_target.text_attachments.size();
  base::string16 message = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(
          IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_MESSAGE, attachment_count),
      {device_name, attachments}, /*offsets=*/nullptr);

  if (transfer_metadata.token()) {
    base::string16 token = l10n_util::GetStringFUTF16(
        IDS_NEARBY_SECURE_CONNECTION_ID,
        base::UTF8ToUTF16(*transfer_metadata.token()));
    message = base::StrCat({message, base::UTF8ToUTF16("\n"), token});
  }

  return message;
}

gfx::Image GetImageFromShareTarget(const ShareTarget& share_target) {
  // TODO(crbug.com/1102348): Create or get profile picture of |share_target|.
  return gfx::Image();
}

NearbyNotificationManager::ReceivedContentType GetReceivedContentType(
    const ShareTarget& share_target) {
  if (!share_target.text_attachments.empty())
    return NearbyNotificationManager::ReceivedContentType::kText;

  if (share_target.file_attachments.size() != 1)
    return NearbyNotificationManager::ReceivedContentType::kFiles;

  const FileAttachment& file = share_target.file_attachments[0];
  if (file.type() == sharing::mojom::FileMetadata::Type::kImage)
    return NearbyNotificationManager::ReceivedContentType::kSingleImage;

  return NearbyNotificationManager::ReceivedContentType::kFiles;
}

class ProgressNotificationDelegate : public NearbyNotificationDelegate {
 public:
  explicit ProgressNotificationDelegate(NearbyNotificationManager* manager)
      : manager_(manager) {}
  ~ProgressNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const base::Optional<int>& action_index) override {
    // Clicking on the notification is a noop.
    if (!action_index)
      return;
    // Clicking on the only (cancel) button cancels the transfer.
    DCHECK_EQ(0, *action_index);
    manager_->CancelTransfer();
  }

  void OnClose(const std::string& notification_id) override {
    manager_->CancelTransfer();
  }

 private:
  NearbyNotificationManager* manager_;
};

class ConnectionRequestNotificationDelegate
    : public NearbyNotificationDelegate {
 public:
  ConnectionRequestNotificationDelegate(NearbyNotificationManager* manager,
                                        bool has_accept_button)
      : manager_(manager), has_accept_button_(has_accept_button) {}
  ~ConnectionRequestNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const base::Optional<int>& action_index) override {
    // Clicking on the notification is a noop.
    if (!action_index)
      return;

    if (!has_accept_button_) {
      DCHECK_EQ(0, *action_index);
      manager_->RejectTransfer();
      return;
    }

    switch (*action_index) {
      case 0:
        manager_->AcceptTransfer();
        break;
      case 1:
        manager_->RejectTransfer();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void OnClose(const std::string& notification_id) override {
    manager_->RejectTransfer();
  }

 private:
  NearbyNotificationManager* manager_;
  bool has_accept_button_;
};

class ReceivedImageDecoder : public ImageDecoder::ImageRequest {
 public:
  using ImageCallback = base::OnceCallback<void(const SkBitmap& decoded_image)>;

  explicit ReceivedImageDecoder(ImageCallback callback)
      : callback_(std::move(callback)) {}
  ~ReceivedImageDecoder() override = default;

  void DecodeImage(const base::Optional<base::FilePath>& image_path) {
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
      NS_LOG(VERBOSE) << __func__ << ": Image contents not found.";
      OnDecodeImageFailed();
      return;
    }

    ImageDecoder::Start(this, *contents);
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
               const base::Optional<int>& action_index) override {
    // Ignore clicks on notification body.
    if (!action_index)
      return;

    switch (type_) {
      case NearbyNotificationManager::ReceivedContentType::kText:
        DCHECK_EQ(0, *action_index);
        CopyTextToClipboard();
        break;
      case NearbyNotificationManager::ReceivedContentType::kSingleImage:
        switch (*action_index) {
          case 0:
            OpenDownloadsFolder();
            break;
          case 1:
            CopyImageToClipboard();
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      case NearbyNotificationManager::ReceivedContentType::kFiles:
        DCHECK_EQ(0, *action_index);
        OpenDownloadsFolder();
        break;
    }

    manager_->CloseSuccessNotification();
  }

  void OnClose(const std::string& notification_id) override {
    manager_->CloseSuccessNotification();
  }

 private:
  void OpenDownloadsFolder() {
    platform_util::OpenItem(
        profile_,
        DownloadPrefs::FromDownloadManager(
            content::BrowserContext::GetDownloadManager(profile_))
            ->DownloadPath(),
        platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());

    if (testing_callback_) {
      std::move(testing_callback_)
          .Run(NearbyNotificationManager::SuccessNotificationAction::
                   kOpenDownloads);
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

  NearbyNotificationManager* manager_;
  Profile* profile_;
  ShareTarget share_target_;
  NearbyNotificationManager::ReceivedContentType type_;
  SkBitmap image_;
  base::OnceCallback<void(NearbyNotificationManager::SuccessNotificationAction)>
      testing_callback_;
};

class OnboardingNotificationDelegate : public NearbyNotificationDelegate {
 public:
  explicit OnboardingNotificationDelegate(NearbyNotificationManager* manager)
      : manager_(manager) {}
  ~OnboardingNotificationDelegate() override = default;

  // NearbyNotificationDelegate:
  void OnClick(const std::string& notification_id,
               const base::Optional<int>& action_index) override {
    manager_->OnOnboardingClicked();
  }

  void OnClose(const std::string& notification_id) override {
    manager_->OnOnboardingDismissed();
  }

 private:
  NearbyNotificationManager* manager_;
};

bool ShouldShowOnboardingNotification(PrefService* pref_service) {
  base::Time last_dismissed = pref_service->GetTime(
      prefs::kNearbySharingOnboardingDismissedTimePrefName);
  if (last_dismissed.is_null())
    return true;

  base::TimeDelta last_dismissed_delta = base::Time::Now() - last_dismissed;
  if (last_dismissed_delta <
      NearbyNotificationManager::kOnboardingDismissedTimeout) {
    NS_LOG(VERBOSE) << "Not showing onboarding notification: the user recently "
                       "dismissed the notification.";
    return false;
  }

  return true;
}

void UpdateOnboardingDismissedTime(PrefService* pref_service) {
  pref_service->SetTime(prefs::kNearbySharingOnboardingDismissedTimePrefName,
                        base::Time::Now());
}

}  // namespace

// static
constexpr base::TimeDelta
    NearbyNotificationManager::kOnboardingDismissedTimeout;

NearbyNotificationManager::NearbyNotificationManager(
    NotificationDisplayService* notification_display_service,
    NearbySharingService* nearby_service,
    PrefService* pref_service,
    Profile* profile)
    : notification_display_service_(notification_display_service),
      nearby_service_(nearby_service),
      pref_service_(pref_service),
      profile_(profile) {
  DCHECK(notification_display_service_);
  DCHECK(nearby_service_);
  DCHECK(pref_service_);
  nearby_service_->RegisterReceiveSurface(
      this, NearbySharingService::ReceiveSurfaceState::kBackground);
  nearby_service_->RegisterSendSurface(
      this, this, NearbySharingService::SendSurfaceState::kBackground);
}

NearbyNotificationManager::~NearbyNotificationManager() {
  nearby_service_->UnregisterReceiveSurface(this);
  nearby_service_->UnregisterSendSurface(this, this);
}

void NearbyNotificationManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby notification manager: "
                  << "Transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(
                         transfer_metadata.status());

  if (!share_target_)
    share_target_ = share_target;
  DCHECK_EQ(share_target_->id, share_target.id);

  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kInProgress:
      ShowProgress(share_target, transfer_metadata);
      break;
    case TransferMetadata::Status::kRejected:
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
    case TransferMetadata::Status::kExternalProviderLaunched:
    case TransferMetadata::Status::kCancelled:
      CloseTransfer();
      break;
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      // Only incoming transfers are handled via notifications.
      if (share_target.is_incoming)
        ShowConnectionRequest(share_target, transfer_metadata);
      break;
    case TransferMetadata::Status::kComplete:
      ShowSuccess(share_target);
      break;
    case TransferMetadata::Status::kTimedOut:
    case TransferMetadata::Status::kFailed:
    case TransferMetadata::Status::kNotEnoughSpace:
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      ShowFailure(share_target);
      break;
    default:
      if (transfer_metadata.is_final_status())
        ShowFailure(share_target);
      break;
  }

  if (transfer_metadata.is_final_status())
    share_target_.reset();
}

void NearbyNotificationManager::OnShareTargetDiscovered(
    ShareTarget share_target) {
  // Nothing to do here.
}

void NearbyNotificationManager::OnShareTargetLost(ShareTarget share_target) {
  // Nothing to do here.
}

void NearbyNotificationManager::ShowProgress(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyNotificationId);
  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification.set_title(GetProgressNotificationTitle(share_target));
  notification.set_never_timeout(true);

  // Show indeterminate progress while waiting for remote device to accept.
  if (transfer_metadata.status() == TransferMetadata::Status::kInProgress)
    notification.set_progress(transfer_metadata.progress());
  else
    notification.set_progress(-1);

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  notification.set_buttons(notification_actions);

  delegate_map_[notification.id()] =
      std::make_unique<ProgressNotificationDelegate>(this);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowConnectionRequest(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyNotificationId);
  notification.set_title(l10n_util::GetStringUTF16(
      IDS_NEARBY_NOTIFICATION_CONNECTION_REQUEST_TITLE));
  notification.set_message(
      GetConnectionRequestNotificationMessage(share_target, transfer_metadata));
  notification.set_icon(GetImageFromShareTarget(share_target));
  notification.set_never_timeout(true);

  bool show_accept_button =
      transfer_metadata.status() ==
      TransferMetadata::Status::kAwaitingLocalConfirmation;

  std::vector<message_center::ButtonInfo> notification_actions;
  if (show_accept_button) {
    notification_actions.emplace_back(
        l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_RECEIVE_ACTION));
  }
  notification_actions.emplace_back(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_DECLINE_ACTION));
  notification.set_buttons(notification_actions);

  delegate_map_[notification.id()] =
      std::make_unique<ConnectionRequestNotificationDelegate>(
          this, show_accept_button);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowOnboarding() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ShouldShowOnboardingNotification(pref_service_))
    return;

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyOnboardingNotificationId);
  notification.set_title(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_TITLE));
  notification.set_message(
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_ONBOARDING_MESSAGE));

  delegate_map_[kNearbyOnboardingNotificationId] =
      std::make_unique<OnboardingNotificationDelegate>(this);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowSuccess(const ShareTarget& share_target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!share_target.is_incoming) {
    message_center::Notification notification =
        CreateNearbyNotification(kNearbyNotificationId);
    notification.set_title(GetSuccessNotificationTitle(share_target));

    delegate_map_.erase(kNearbyNotificationId);

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
  message_center::Notification notification =
      CreateNearbyNotification(kNearbyNotificationId);
  notification.set_title(GetSuccessNotificationTitle(share_target));

  // Revert to generic file handling if image decoding failed.
  if (type == ReceivedContentType::kSingleImage && image.isNull())
    type = ReceivedContentType::kFiles;

  if (!image.isNull()) {
    notification.set_type(message_center::NOTIFICATION_TYPE_IMAGE);
    notification.set_image(gfx::Image::CreateFrom1xBitmap(image));
  }

  std::vector<message_center::ButtonInfo> notification_actions;
  switch (type) {
    case ReceivedContentType::kText:
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
  }
  notification.set_buttons(notification_actions);

  delegate_map_[kNearbyNotificationId] =
      std::make_unique<SuccessNotificationDelegate>(
          this, profile_, share_target, type, image,
          std::move(success_action_test_callback_));

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::ShowFailure(const ShareTarget& share_target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::Notification notification =
      CreateNearbyNotification(kNearbyNotificationId);
  notification.set_title(GetFailureNotificationTitle(share_target));

  delegate_map_.erase(kNearbyNotificationId);

  notification_display_service_->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}

void NearbyNotificationManager::CloseTransfer() {
  delegate_map_.erase(kNearbyNotificationId);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       kNearbyNotificationId);
}

void NearbyNotificationManager::CloseOnboarding() {
  delegate_map_.erase(kNearbyOnboardingNotificationId);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       kNearbyOnboardingNotificationId);
}

NearbyNotificationDelegate* NearbyNotificationManager::GetNotificationDelegate(
    const std::string& notification_id) {
  auto iter = delegate_map_.find(notification_id);
  if (iter == delegate_map_.end())
    return nullptr;

  return iter->second.get();
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

void NearbyNotificationManager::OnOnboardingClicked() {
  CloseOnboarding();
  // TODO(crbug.com/1102348): Start user onboarding or high visibility if user
  // has been onboarded already.
}

void NearbyNotificationManager::OnOnboardingDismissed() {
  CloseOnboarding();
  UpdateOnboardingDismissedTime(pref_service_);
}

void NearbyNotificationManager::CloseSuccessNotification() {
  delegate_map_.erase(kNearbyNotificationId);
  notification_display_service_->Close(NotificationHandler::Type::NEARBY_SHARE,
                                       kNearbyNotificationId);
}

void NearbyNotificationManager::SetOnSuccessClickedForTesting(
    base::OnceCallback<void(SuccessNotificationAction)> callback) {
  success_action_test_callback_ = std::move(callback);
}
