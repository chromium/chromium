// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_image_fetcher.h"

#include <memory>

#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/functional/callback.h"

LobsterImageFetcher::LobsterImageFetcher(std::unique_ptr<Provider> provider)
    : provider_(std::move(provider)) {
  CHECK(provider_);
}

LobsterImageFetcher::~LobsterImageFetcher() = default;

void LobsterImageFetcher::RequestCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  provider_->RequestMultipleCandidates(query, num_candidates,
                                       std::move(callback));
}

void LobsterImageFetcher::RequestFullSizeCandidate(
    const std::string& query,
    uint32_t seed,
    ash::RequestCandidatesCallback callback) {
  provider_->RequestSingleCandidateWithSeed(query, seed, std::move(callback));
}

void LobsterImageFetcher::SetProvider(std::unique_ptr<Provider> provider) {
  provider_ = std::move(provider);
}
