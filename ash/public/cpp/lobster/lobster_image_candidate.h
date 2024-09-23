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
  std::string query;

  bool operator==(const LobsterImageCandidate& other) const {
    return id == other.id && image_bytes == other.image_bytes &&
           seed == other.seed && query == other.query;
  }
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_CANDIDATE_H_
