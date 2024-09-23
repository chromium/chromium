// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_RESULT_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_RESULT_H_

#include <string>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/types/expected.h"

namespace ash {

struct LobsterError {
  LobsterErrorCode error_code;
  std::string message;

  bool operator==(const LobsterError& other) const {
    return error_code == other.error_code && message == other.message;
  }
};

using LobsterResult =
    base::expected<std::vector<LobsterImageCandidate>, LobsterError>;

using RequestCandidatesCallback =
    base::OnceCallback<void(const LobsterResult&)>;

using InflateCandidateCallback = base::OnceCallback<void(const LobsterResult&)>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_RESULT_H_
