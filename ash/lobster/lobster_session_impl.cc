// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/lobster/lobster_image_download_actuator.h"
#include "ash/lobster/lobster_image_insert_or_copy_actuator.h"
#include "ash/lobster/lobster_metrics_recorder.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "components/feedback/feedback_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
namespace ash {

namespace {

constexpr char kLobsterSuccessfulImageDownloadNotifierId[] =
    "ash.lobster_successful_image_download_notifier_id";
constexpr char kLobsterFailedImageDownloadNotifierId[] =
    "ash.lobster_failed_image_download_notifier_id";
constexpr char kLobsterSuccessfulImageDownloadNotificationId[] =
    "lobster_successful_image_download_notification_id";
constexpr char kLobsterFailedImageDownloadNotificationId[] =
    "lobster_failed_image_download_notification_id";

std::u16string GetDownloadNotificationSourceLabel() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_IMAGE_DOWNLOAD_NOTIFICATION_SOURCE);
}

std::u16string GetSuccessfulImageDownloadNotificationTitle() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_SUCCESSFUL_IMAGE_DOWNLOAD_NOTIFICATION_TITLE);
}

std::u16string GetFailedImageDownloadNotificationTitle(
    const std::string& file_name) {
  return l10n_util::GetStringFUTF16(
      IDS_LOBSTER_FAILED_IMAGE_DOWNLOAD_NOTIFICATION_TITLE,
      base::UTF8ToUTF16(file_name));
}

std::u16string GetFailedImageDownloadNotificationMessage() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_FAILED_IMAGE_DOWNLOAD_NOTIFICATION_MESSAGE);
}

std::u16string GetShowInFolderButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_SUCCESSFUL_IMAGE_DOWNLOAD_NOTIFICATION_SHOW_IN_FOLDER_ACTION_LABEL);
}

std::u16string GetCopyToClipboardButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_SUCCESSFUL_IMAGE_DOWNLOAD_NOTIFICATION_COPY_IMAGE_TO_CLIPBOARD_ACTION_LABEL);
}

std::u16string GetAnnouncementForInsertionSuccess() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_IMAGE_INSERTION_ANNOUNCEMENT_SUCCESS);
}

std::u16string GetAnnouncementForInsertionFailure() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_IMAGE_INSERTION_ANNOUNCEMENT_FAILURE);
}

std::string BuildFeedbackDescription(std::string_view query,
                                     std::string_view model_version,
                                     std::string_view user_description) {
  return base::StringPrintf(
      "model_input: %s\nmodel_version: %s\nuser_description: %s", query,
      model_version, user_description);
}

void OpenDownloadsFolder() {
  ash::NewWindowDelegate::GetInstance()->OpenDownloadsFolder();
}

message_center::RichNotificationData CreateRichNotificationData(
    const base::FilePath& image_path,
    const std::string& image_bytes) {
  message_center::RichNotificationData rich_notification_data;

  rich_notification_data.image =
      gfx::ImageFrom1xJPEGEncodedData(base::as_byte_span(image_bytes));
  rich_notification_data.image_path = image_path;
  rich_notification_data.buttons.emplace_back(GetShowInFolderButtonLabel());
  rich_notification_data.buttons.emplace_back(GetCopyToClipboardButtonLabel());

  return rich_notification_data;
}

void DisplaySuccessfulImageDownloadNotification(
    const base::FilePath& image_path,
    const std::string& image_bytes) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          /*type=*/message_center::NOTIFICATION_TYPE_IMAGE,
          /*id=*/kLobsterSuccessfulImageDownloadNotificationId,
          /*title=*/GetSuccessfulImageDownloadNotificationTitle(),
          /*message=*/base::UTF8ToUTF16(image_path.BaseName().value()),
          /*display_source=*/GetDownloadNotificationSourceLabel(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kLobsterSuccessfulImageDownloadNotifierId,
              NotificationCatalogName::kDownloadImageFromLobster),
          CreateRichNotificationData(image_path, image_bytes),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  [](const std::string& image_bytes,
                     std::optional<int> button_index) {
                    if (!button_index.has_value()) {
                      return;
                    }
                    CHECK(*button_index == 0 || button_index == 1);

                    if (button_index == 0) {
                      OpenDownloadsFolder();
                    } else if (button_index == 1) {
                      CopyToClipboard(image_bytes);
                    }
                  },
                  image_bytes)),
          /*small_image=*/vector_icons::kFileDownloadIcon,
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::NORMAL);

  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

void DisplayFailedImageDownloadNotification(const base::FilePath& image_path) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          /*type=*/message_center::NOTIFICATION_TYPE_IMAGE,
          /*id=*/kLobsterFailedImageDownloadNotificationId,
          /*title=*/
          GetFailedImageDownloadNotificationTitle(
              image_path.BaseName().value()),
          /*message=*/GetFailedImageDownloadNotificationMessage(),
          /*display_source=*/GetDownloadNotificationSourceLabel(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kLobsterFailedImageDownloadNotifierId,
              NotificationCatalogName::kDownloadImageFromLobster),
          message_center::RichNotificationData(),
          /*delegate=*/nullptr,
          /*small_image=*/vector_icons::kFileDownloadIcon,
          /*warning_level=*/
          message_center::SystemNotificationWarningLevel::NORMAL);

  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

void AnnounceInsertionResultLater(LobsterClient* client, bool success) {
  client->AnnounceLater(success ? GetAnnouncementForInsertionSuccess()
                                : GetAnnouncementForInsertionFailure());
}

}  // namespace

LobsterSessionImpl::LobsterSessionImpl(
    std::unique_ptr<LobsterClient> client,
    const LobsterCandidateStore& candidate_store,
    LobsterEntryPoint entry_point,
    LobsterMode mode)
    : client_(std::move(client)),
      candidate_store_(candidate_store),
      entry_point_(entry_point),
      mode_(mode) {
  switch (entry_point_) {
    case LobsterEntryPoint::kQuickInsert:
      RecordLobsterState(LobsterMetricState::kQuickInsertTriggerFired);
      break;
    case LobsterEntryPoint::kRightClickMenu:
      RecordLobsterState(LobsterMetricState::kRightClickTriggerFired);
      break;
  }
}

LobsterSessionImpl::LobsterSessionImpl(std::unique_ptr<LobsterClient> client,
                                       LobsterEntryPoint entry_point,
                                       LobsterMode mode)
    : LobsterSessionImpl(std::move(client),
                         LobsterCandidateStore(),
                         entry_point,
                         mode) {}

LobsterSessionImpl::~LobsterSessionImpl() = default;

void LobsterSessionImpl::DownloadCandidate(int candidate_id,
                                           const base::FilePath& download_dir,
                                           StatusCallback status_callback) {
  RecordLobsterState(LobsterMetricState::kCandidateDownload);

  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);

  if (!candidate.has_value()) {
    LOG(ERROR) << "No candidate found.";
    std::move(status_callback).Run(false);
    RecordLobsterState(LobsterMetricState::kCandidateDownloadError);
    return;
  }

  client_->InflateCandidate(
      candidate->seed,
      ash::features::IsLobsterUseRewrittenQuery() ? candidate->rewritten_query
                                                  : candidate->user_query,
      base::BindOnce(
          [](LobsterClient* lobster_client,
             LobsterImageDownloadActuator* actuator,
             const base::FilePath& download_dir, const std::string& file_name,
             StatusCallback status_callback, const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              RecordLobsterState(LobsterMetricState::kCandidateDownloadError);
              return;
            }

            const LobsterImageCandidate& image_candidate = (*result)[0];
            actuator->WriteImageToPath(
                download_dir, file_name, image_candidate.id,
                image_candidate.image_bytes,
                base::BindOnce(
                    [](StatusCallback status_callback,
                       const std::string& image_bytes,
                       const LobsterImageDownloadResponse& download_response) {
                      std::move(status_callback).Run(download_response.success);

                      if (download_response.success) {
                        DisplaySuccessfulImageDownloadNotification(
                            download_response.download_path, image_bytes);
                        RecordLobsterState(
                            LobsterMetricState::kCandidateDownloadSuccess);
                        return;
                      }

                      DisplayFailedImageDownloadNotification(
                          download_response.download_path);
                      RecordLobsterState(
                          LobsterMetricState::kCandidateDownloadError);
                    },
                    std::move(status_callback), image_candidate.image_bytes));
          },
          client_.get(), &download_actuator_, download_dir,
          // Always use the original user query for the filename
          candidate->user_query, std::move(status_callback)));
}

void LobsterSessionImpl::RequestCandidates(const std::string& query,
                                           int num_candidates,
                                           RequestCandidatesCallback callback) {
  client_->RequestCandidates(
      query, num_candidates,
      base::BindOnce(&LobsterSessionImpl::OnRequestCandidates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LobsterSessionImpl::CommitAsInsert(int candidate_id,
                                        StatusCallback status_callback) {
  RecordLobsterState(LobsterMetricState::kCommitAsInsert);

  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);

  if (!candidate.has_value()) {
    LOG(ERROR) << "No candidate found.";
    std::move(status_callback).Run(false);
    AnnounceInsertionResultLater(client_.get(), false);
    RecordLobsterState(LobsterMetricState::kCommitAsInsertError);
    return;
  }

  client_->InflateCandidate(
      candidate->seed,
      ash::features::IsLobsterUseRewrittenQuery() ? candidate->rewritten_query
                                                  : candidate->user_query,
      base::BindOnce(
          [](LobsterClient* lobster_client, StatusCallback status_callback,
             const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              AnnounceInsertionResultLater(lobster_client, false);
              RecordLobsterState(LobsterMetricState::kCommitAsInsertError);
              return;
            }

            // Queue the data to be inserted later.
            lobster_client->QueueInsertion(
                (*result)[0].image_bytes, base::BindOnce([](bool success) {
                  RecordLobsterState(
                      success ? LobsterMetricState::kCommitAsInsertSuccess
                              : LobsterMetricState::kCommitAsInsertError);
                }));

            // We only know whether the insertion is successful or not after the
            // webui is closed. Therefore, as long as the inflation request is
            // successful, we return true back to WebUI and close WebUI.
            std::move(status_callback).Run(true);
            AnnounceInsertionResultLater(lobster_client, true);

            // Close the WebUI.
            lobster_client->CloseUI();
          },
          client_.get(), std::move(status_callback)));
}

void LobsterSessionImpl::CommitAsDownload(int candidate_id,
                                          const base::FilePath& download_dir,
                                          StatusCallback status_callback) {
  RecordLobsterState(LobsterMetricState::kCommitAsDownload);

  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);

  if (!candidate.has_value()) {
    LOG(ERROR) << "No candidate found.";
    std::move(status_callback).Run(false);
    RecordLobsterState(LobsterMetricState::kCommitAsDownloadError);
    return;
  }

  client_->InflateCandidate(
      candidate->seed,
      ash::features::IsLobsterUseRewrittenQuery() ? candidate->rewritten_query
                                                  : candidate->user_query,
      base::BindOnce(
          [](LobsterClient* lobster_client,
             LobsterImageDownloadActuator* actuator,
             const base::FilePath& download_dir, const std::string& file_name,
             StatusCallback status_callback, const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              RecordLobsterState(LobsterMetricState::kCommitAsDownloadError);
              return;
            }

            const LobsterImageCandidate& image_candidate = (*result)[0];
            actuator->WriteImageToPath(
                download_dir, file_name, image_candidate.id,
                image_candidate.image_bytes,
                base::BindOnce(
                    [](LobsterClient* lobster_client,
                       const std::string& image_bytes,
                       StatusCallback status_callback,
                       const LobsterImageDownloadResponse& download_response) {
                      std::move(status_callback).Run(download_response.success);
                      // Close the WebUI.
                      lobster_client->CloseUI();

                      if (download_response.success) {
                        DisplaySuccessfulImageDownloadNotification(
                            download_response.download_path, image_bytes);
                        RecordLobsterState(
                            LobsterMetricState::kCommitAsDownloadSuccess);
                        return;
                      }

                      DisplayFailedImageDownloadNotification(
                          download_response.download_path);
                      RecordLobsterState(
                          LobsterMetricState::kCommitAsDownloadError);
                    },
                    lobster_client, image_candidate.image_bytes,
                    std::move(status_callback)));
          },
          client_.get(), &download_actuator_, download_dir,
          // Always use the original user query for the filename
          candidate->user_query, std::move(status_callback)));
}

void LobsterSessionImpl::PreviewFeedback(
    int candidate_id,
    LobsterPreviewFeedbackCallback callback) {
  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);
  if (!candidate.has_value()) {
    std::move(callback).Run(base::unexpected("No candidate found."));
    return;
  }

  std::move(callback).Run(LobsterFeedbackPreview(
      {{"Query and image", candidate->user_query}}, candidate->image_bytes));
}

bool LobsterSessionImpl::SubmitFeedback(int candidate_id,
                                        const std::string& description) {
  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);
  if (!candidate.has_value()) {
    return false;
  }
  // Submit feedback along with the preview image.
  // TODO: b/362403784 - add the proper version.
  std::string feedback_description = BuildFeedbackDescription(
      candidate->user_query, /*model_version=*/"dummy_version", description);

  return Shell::Get()->shell_delegate()->SendSpecializedFeatureFeedback(
      client_->GetAccountId(), feedback::kLobsterFeedbackProductId,
      std::move(feedback_description), std::move(candidate->image_bytes),
      /*image_mime_type=*/std::nullopt);
}

void LobsterSessionImpl::OnRequestCandidates(RequestCandidatesCallback callback,
                                             const LobsterResult& result) {
  if (result.has_value()) {
    for (auto& image_candidate : *result) {
      candidate_store_.Cache(image_candidate);
    }
  }
  std::move(callback).Run(result);
}

void LobsterSessionImpl::LoadUIFromCachedContext() {
  client_->LoadUI(query_before_disclaimer_ui_, /*mode=*/mode_,
                  /*anchor_bounds=*/anchor_bounds_before_disclaimer_ui_);
}

void LobsterSessionImpl::LoadUI(std::optional<std::string> query,
                                LobsterMode mode,
                                const gfx::Rect& caret_bounds) {
  client_->LoadUI(query, mode, caret_bounds);
}

void LobsterSessionImpl::ShowDisclaimerUIAndCacheContext(
    std::optional<std::string> query,
    const gfx::Rect& anchor_bounds) {
  client_->ShowDisclaimerUI();
  RecordLobsterState(ash::LobsterMetricState::kConsentScreenImpression);

  query_before_disclaimer_ui_ = query;
  anchor_bounds_before_disclaimer_ui_ = anchor_bounds;
}

void LobsterSessionImpl::ShowUI() {
  client_->ShowUI();
}

void LobsterSessionImpl::CloseUI() {
  client_->CloseUI();
}

void LobsterSessionImpl::RecordWebUIMetricEvent(
    ash::LobsterMetricState metric_event) {
  RecordLobsterState(metric_event);
}

}  // namespace ash
