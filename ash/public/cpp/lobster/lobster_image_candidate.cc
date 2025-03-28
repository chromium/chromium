// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lobster/lobster_image_candidate.h"

namespace ash {

LobsterImageCandidate::LobsterImageCandidate(uint32_t id,
                                             const std::string& image_bytes,
                                             uint32_t seed,
                                             const std::string& user_query,
                                             const std::string& rewritten_query)
    : id(id),
      image_bytes(image_bytes),
      seed(seed),
      user_query(user_query),
      rewritten_query(rewritten_query) {}

LobsterImageCandidate::LobsterImageCandidate() = default;

LobsterImageCandidate& LobsterImageCandidate::operator=(
    const LobsterImageCandidate&) = default;

LobsterImageCandidate::LobsterImageCandidate(const LobsterImageCandidate&) =
    default;

}  // namespace ash
