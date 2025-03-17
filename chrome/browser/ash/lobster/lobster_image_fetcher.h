// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_FETCHER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_FETCHER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/lobster/lobster_result.h"

// The interface for lobster to fetch image candidates.
class LobsterImageFetcher {
 public:
  class Provider {
   public:
    virtual ~Provider() = default;

    // Requests multiple image candidates for the given `query`.
    virtual void RequestMultipleCandidates(
        const std::string& query,
        int num_candidates,
        ash::RequestCandidatesCallback callback) = 0;

    // Requests single image candidate from the given `seed`.
    virtual void RequestSingleCandidateWithSeed(
        const std::string& query,
        uint32_t seed,
        ash::RequestCandidatesCallback callback) = 0;
  };

  explicit LobsterImageFetcher(std::unique_ptr<Provider> provider);

  LobsterImageFetcher(const LobsterImageFetcher&) = delete;
  LobsterImageFetcher& operator=(const LobsterImageFetcher&) = delete;

  ~LobsterImageFetcher();

  // Requests a list of image candidates for the given `query`.
  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         ash::RequestCandidatesCallback callback);

  // Requests the full-size image for the candidate specified by `query` and
  // `seed`.
  void RequestFullSizeCandidate(const std::string& query,
                                uint32_t seed,
                                ash::RequestCandidatesCallback callback);

  void SetProvider(std::unique_ptr<Provider> provider);

 private:
  std::unique_ptr<Provider> provider_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_FETCHER_H_
