// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/lobster/lobster_image_actuator.h"
#include "ash/lobster/lobster_metrics_recorder.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method.h"

namespace ash {

namespace {

ui::TextInputClient* GetFocusedTextInputClient() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method || !input_method->GetTextInputClient()) {
    return nullptr;
  }
  return input_method->GetTextInputClient();
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
                                           const base::FilePath& file_path,
                                           StatusCallback status_callback) {
  RecordLobsterState(LobsterMetricState::kCandidateDownload);
  InflateCandidateAndPerformAction(
      candidate_id, base::BindOnce(&WriteImageToPath, file_path),
      base::BindOnce([](bool success) {
        RecordLobsterState(success
                               ? LobsterMetricState::kCandidateDownloadSuccess
                               : LobsterMetricState::kCandidateDownloadError);
        return success;
      }).Then(std::move(status_callback)));
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
  InflateCandidateAndPerformAction(candidate_id,
                                   base::BindOnce(&InsertImageOrCopyToClipboard,
                                                  GetFocusedTextInputClient()),
                                   base::BindOnce([](bool success) {
                                     // TODO: b:348514607 - Adds any logic to
                                     // execute before returning the status back
                                     // to WebUI, e.g. recording commit as
                                     // insert metric.
                                     return success;
                                   }).Then(std::move(status_callback)));
}

void LobsterSessionImpl::CommitAsDownload(int candidate_id,
                                          const base::FilePath& file_path,
                                          StatusCallback status_callback) {
  InflateCandidateAndPerformAction(candidate_id,
                                   base::BindOnce(&WriteImageToPath, file_path),
                                   base::BindOnce([](bool success) {
                                     // TODO: b:348514797 - Adds any logic to
                                     // execute before returning the status back
                                     // to WebUI, e.g. recording commit as
                                     // download metric.
                                     return success;
                                   }).Then(std::move(status_callback)));
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

void LobsterSessionImpl::InflateCandidateAndPerformAction(
    int candidate_id,
    ActionCallback action_callback,
    StatusCallback status_callback) {
  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);
  if (!candidate.has_value()) {
    LOG(ERROR) << "No candidate found.";
    std::move(status_callback).Run(false);
    return;
  }

  client_->InflateCandidate(
      candidate->seed, candidate->query,
      base::BindOnce(
          [](ActionCallback action_callback, StatusCallback status_callback,
             const LobsterResult& result) {
            if (!result.has_value() || result->size() == 0) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              return;
            }
            std::move(action_callback)
                .Run((*result)[0].image_bytes, std::move(status_callback));
          },
          std::move(action_callback), std::move(status_callback)));
}

void LobsterSessionImpl::LoadUI(std::optional<std::string> query) {
  client_->LoadUI(query);
}

void LobsterSessionImpl::ShowUI() {
  client_->ShowUI();
}

void LobsterSessionImpl::CloseUI() {
  client_->CloseUI();
}

}  // namespace ash
