// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_
#define ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"

namespace ash {

class LobsterClient;

class ASH_EXPORT LobsterSessionImpl : public LobsterSession {
 public:
  explicit LobsterSessionImpl(std::unique_ptr<LobsterClient> client);
  ~LobsterSessionImpl() override;

  // LobsterSession overrides
  void DownloadCandidate(int candidate_id, StatusCallback callback) override;

 private:
  std::unique_ptr<LobsterClient> client_;
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_
