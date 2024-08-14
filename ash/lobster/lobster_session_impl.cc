// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/logging.h"

namespace ash {

LobsterSessionImpl::LobsterSessionImpl(std::unique_ptr<LobsterClient> client)
    : client_(std::move(client)) {
  client_->SetActiveSession(this);
}

LobsterSessionImpl::~LobsterSessionImpl() {
  client_->SetActiveSession(nullptr);
}

void LobsterSessionImpl::DownloadCandidate(int candidate_id,
                                           StatusCallback callback) {
  std::optional<LobsterImageCandidate> candidate =
      candidate_store_.FindCandidateById(candidate_id);
  if (!candidate.has_value()) {
    LOG(ERROR) << "No candidate found.";
    std::move(callback).Run(false);
    return;
  }

  client_->InflateCandidate(
      candidate->seed, candidate->query,
      base::BindOnce(
          [](StatusCallback status_callback,
             std::optional<LobsterImageCandidate> image_candidate) {
            if (!image_candidate.has_value()) {
              LOG(ERROR) << "No image candidate";
              std::move(status_callback).Run(false);
              return;
            }

            // TODO: b:348283703 - Add download logic here.
            std::move(status_callback).Run(true);
          },
          std::move(callback)));
}

void LobsterSessionImpl::RequestCandidates(const std::string& query,
                                           int num_candidates,
                                           RequestCandidatesCallback callback) {
  client_->RequestCandidates(
      query, num_candidates,
      base::BindOnce(&LobsterSessionImpl::OnRequestCandidates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LobsterSessionImpl::OnRequestCandidates(
    RequestCandidatesCallback callback,
    const std::vector<LobsterImageCandidate>& image_candidates) {
  for (auto& image_candidate : image_candidates) {
    candidate_store_.Cache(image_candidate);
  }

  std::move(callback).Run(image_candidates);
}

}  // namespace ash
