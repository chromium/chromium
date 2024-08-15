// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/lobster/lobster_image_actuator.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/logging.h"
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

LobsterSessionImpl::LobsterSessionImpl(std::unique_ptr<LobsterClient> client)
    : client_(std::move(client)) {
  client_->SetActiveSession(this);
}

LobsterSessionImpl::~LobsterSessionImpl() {
  client_->SetActiveSession(nullptr);
}

void LobsterSessionImpl::DownloadCandidate(int candidate_id,
                                           StatusCallback status_callback) {
  // TODO: b:348283703 - Add download logic here.
  InflateCandidateAndPerformAction(candidate_id,
                                   base::BindOnce([](const std::string&) {}),
                                   std::move(status_callback));
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
  InflateCandidateAndPerformAction(
      candidate_id, base::BindOnce([](const std::string& image_bytes) {
        LobsterImageActuator image_actuator;
        image_actuator.InsertImageOrCopyToClipboard(GetFocusedTextInputClient(),
                                                    image_bytes);
      }),
      std::move(status_callback));
}

void LobsterSessionImpl::CommitAsDownload(int candidate_id,
                                          StatusCallback status_callback) {
  // TODO: b:348283703 - Add commit as download logic here.
  InflateCandidateAndPerformAction(candidate_id,
                                   base::BindOnce([](const std::string&) {}),
                                   std::move(status_callback));
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
          [](ActionCallback action_callback, const LobsterResult& result) {
            if (!result.has_value()) {
              LOG(ERROR) << "No image candidate";
              return false;
            }

            // TODO: b/348283703 - Return the value of action callback.
            std::move(action_callback).Run((*result)[0].image_bytes);
            return true;
          },
          std::move(action_callback))
          .Then(base::BindOnce(
              [](StatusCallback status_callback, bool success) {
                std::move(status_callback).Run(success);
              },
              std::move(status_callback))));
}

}  // namespace ash
