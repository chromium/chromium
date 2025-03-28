// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_CANDIDATE_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_CANDIDATE_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"

namespace ash {

struct ASH_PUBLIC_EXPORT LobsterImageCandidate {
  uint32_t id;
  std::string image_bytes;
  uint32_t seed;
  std::string user_query;
  // In case Query Rewritten feature is enabled, the `rewritten_query` is
  // returned by the server. Otherwise, it will be the same as the `user_query`.
  std::string rewritten_query;

  LobsterImageCandidate(uint32_t id,
                        const std::string& image_bytes,
                        uint32_t seed,
                        const std::string& user_query,
                        const std::string& rewritten_query);

  LobsterImageCandidate();

  LobsterImageCandidate(const LobsterImageCandidate&);
  LobsterImageCandidate& operator=(const LobsterImageCandidate&);

  bool operator==(const LobsterImageCandidate& other) const {
    return id == other.id && image_bytes == other.image_bytes &&
           seed == other.seed && user_query == other.user_query &&
           rewritten_query == other.rewritten_query;
  }
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_CANDIDATE_H_
