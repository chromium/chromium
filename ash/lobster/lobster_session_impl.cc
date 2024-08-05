// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <memory>
#include <string_view>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"

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
  std::move(callback).Run(false);
}

void LobsterSessionImpl::RequestCandidates(std::string_view query,
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
