// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/lobster/lobster_result.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/functional/callback.h"

namespace ash {

class ASH_PUBLIC_EXPORT LobsterClient {
 public:
  virtual ~LobsterClient() = default;

  virtual void SetActiveSession(LobsterSession* session) = 0;
  virtual LobsterSystemState GetSystemState() = 0;
  virtual void RequestCandidates(const std::string& query,
                                 int num_candidates,
                                 RequestCandidatesCallback) = 0;
  virtual void InflateCandidate(uint32_t seed,
                                const std::string& query,
                                InflateCandidateCallback) = 0;
  virtual bool SubmitFeedback(const std::string& query,
                              const std::string& model_version,
                              const std::string& description,
                              const std::string& image_bytes) = 0;
  virtual void LoadUI(std::optional<std::string> query) = 0;
  virtual void ShowUI() = 0;
  virtual void CloseUI() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_
