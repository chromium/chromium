// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_MEMORY_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_MEMORY_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/ash/lobster/lobster_image_fetcher.h"

class LobsterImageProviderFromMemory : public LobsterImageFetcher::Provider {
 public:
  explicit LobsterImageProviderFromMemory(
      LobsterCandidateIdGenerator* id_generator);
  ~LobsterImageProviderFromMemory() override;

  void RequestMultipleCandidates(
      const std::string& query,
      int num_candidates,
      ash::RequestCandidatesCallback callback) override;

  void RequestSingleCandidateWithSeed(
      const std::string& query,
      uint32_t seed,
      ash::RequestCandidatesCallback callback) override;

 private:
  raw_ptr<LobsterCandidateIdGenerator> id_generator_;
  base::OneShotTimer delay_timer_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_MEMORY_H_
