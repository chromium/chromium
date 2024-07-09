// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session.h"

#include <memory>

#include "ash/public/cpp/lobster/lobster_client.h"

namespace ash {

LobsterSession::LobsterSession(std::unique_ptr<LobsterClient> client)
    : client_(std::move(client)), system_state_(client_->GetSystemState()) {}

LobsterSession::~LobsterSession() = default;

LobsterStatus LobsterSession::GetStatus() {
  return system_state_.status;
}

}  // namespace ash
