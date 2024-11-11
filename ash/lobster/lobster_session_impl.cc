// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/lobster/lobster_image_actuator.h"
#include "ash/lobster/lobster_metrics_recorder.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"

namespace ash {

namespace {

constexpr int kQueryCharLimit = 230;

std::string BuildDownloadFileName(const std::string& query, uint32_t id) {
  std::string sanitized_file_name = query;

  base::i18n::ReplaceIllegalCharactersInPath(&sanitized_file_name, '-');

  return base::StringPrintf("%s-%d.jpeg",
                            sanitized_file_name.substr(0, kQueryCharLimit), id);
}

base::FilePath CreateDownloadFilePath(const base::FilePath& download_dir,
                                      const std::string& file_name) {
  return download_dir.Append(
      base::FeatureList::IsEnabled(features::kLobsterFileNamingImprovement)
          ? file_name
          : "");
}

}  // namespace

LobsterSessionImpl::LobsterSessionImpl(
    std::unique_ptr<LobsterClient> client,
    const LobsterCandidateStore& candidate_store,
    LobsterEntryPoint entry_point)
    : client_(std::move(client)),
      candidate_store_(candidate_store),
      entry_point_(entry_point) {
  switch (entry_point_) {
    case LobsterEntryPoint::kPicker:
      RecordLobsterState(LobsterMetricState::kPickerTriggerFired);
      break;
    case LobsterEntryPoint::kRightClickMenu:
      RecordLobsterState(LobsterMetricState::kRightClickTriggerFired);
      break;
  }
}

LobsterSessionImpl::LobsterSessionImpl(std::unique_ptr<LobsterClient> client,
                                       LobsterEntryPoint entry_point)
    : LobsterSessionImpl(std::move(client),
                         LobsterCandidateStore(),
                         entry_point) {}

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
      candidate->seed, candidate->query,
      base::BindOnce(
          [](LobsterClient* lobster_client, const base::FilePath& download_dir,
             StatusCallback status_callback, const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              RecordLobsterState(LobsterMetricState::kCandidateDownloadError);
              return;
            }

            const LobsterImageCandidate& image_candidate = (*result)[0];

            WriteImageToPath(
                CreateDownloadFilePath(
                    download_dir, BuildDownloadFileName(image_candidate.query,
                                                        image_candidate.id)),
                image_candidate.image_bytes,
                base::BindOnce(
                    [](StatusCallback status_callback, bool success) {
                      std::move(status_callback).Run(success);
                      RecordLobsterState(
                          success
                              ? LobsterMetricState::kCandidateDownloadSuccess
                              : LobsterMetricState::kCandidateDownloadError);
                    },
                    std::move(status_callback)));
          },
          client_.get(), download_dir, std::move(status_callback)));
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
    RecordLobsterState(LobsterMetricState::kCommitAsInsertError);
    return;
  }

  client_->InflateCandidate(
      candidate->seed, candidate->query,
      base::BindOnce(
          [](LobsterClient* lobster_client, StatusCallback status_callback,
             const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
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
      candidate->seed, candidate->query,
      base::BindOnce(
          [](LobsterClient* lobster_client, const base::FilePath& download_dir,
             StatusCallback status_callback, const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              RecordLobsterState(LobsterMetricState::kCommitAsDownloadError);
              return;
            }

            const LobsterImageCandidate& image_candidate = (*result)[0];

            WriteImageToPath(
                CreateDownloadFilePath(
                    download_dir, BuildDownloadFileName(image_candidate.query,
                                                        image_candidate.id)),
                image_candidate.image_bytes,
                base::BindOnce(
                    [](LobsterClient* lobster_client,
                       StatusCallback status_callback, bool success) {
                      std::move(status_callback).Run(success);
                      // Close the WebUI.
                      lobster_client->CloseUI();
                      RecordLobsterState(
                          success ? LobsterMetricState::kCommitAsDownloadSuccess
                                  : LobsterMetricState::kCommitAsDownloadError);
                    },
                    lobster_client, std::move(status_callback)));
          },
          client_.get(), download_dir, std::move(status_callback)));
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

  // TODO: b/362403784 - add the proper version.
  std::move(callback).Run(LobsterFeedbackPreview(
      {{"model_version", "dummy_version"}, {"model_input", candidate->query}},
      candidate->image_bytes));
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
  return client_->SubmitFeedback(/*query=*/candidate->query,
                                 /*model_version=*/"dummy_version",
                                 /*description=*/description,
                                 /*image_bytes=*/candidate->image_bytes);
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

void LobsterSessionImpl::LoadUI(std::optional<std::string> query,
                                LobsterMode mode) {
  client_->LoadUI(query, mode);
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
