// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <memory>

#include "ash/public/cpp/lobster/lobster_client.h"

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

}  // namespace ash
