// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_candidate_store.h"

#include <optional>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"

namespace ash {

LobsterCandidateStore::LobsterCandidateStore() = default;

LobsterCandidateStore::LobsterCandidateStore(
    const LobsterCandidateStore& other) = default;

LobsterCandidateStore::~LobsterCandidateStore() = default;

void LobsterCandidateStore::Cache(const LobsterImageCandidate& candidate) {
  cache_[candidate.id] = candidate;
}

std::optional<LobsterImageCandidate> LobsterCandidateStore::FindCandidateById(
    uint32_t id) {
  if (cache_.find(id) != cache_.end()) {
    return cache_[id];
  } else {
    return std::nullopt;
  }
}

}  // namespace ash
