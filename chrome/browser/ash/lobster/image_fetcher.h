// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_IMAGE_FETCHER_H_
#define CHROME_BROWSER_ASH_LOBSTER_IMAGE_FETCHER_H_

#include <optional>
#include <string>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/gfx/geometry/size.h"

namespace manta {

namespace proto {
class Response;
}  // namespace proto

class SnapperProvider;
struct MantaStatus;
}  // namespace manta

class ImageFetcher {
 public:
  explicit ImageFetcher(manta::SnapperProvider* provider,
                        LobsterCandidateIdGenerator* id_generator);
  ~ImageFetcher();

  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         ash::RequestCandidatesCallback);

  void RequestFullSizeCandidate(const std::string& query,
                                uint32_t seed,
                                ash::RequestCandidatesCallback);

 private:
  void OnCandidatesRequested(const std::string& query,
                             ash::RequestCandidatesCallback,
                             std::unique_ptr<manta::proto::Response> response,
                             manta::MantaStatus status);

  void OnImagesSanitized(
      ash::RequestCandidatesCallback callback,
      const std::vector<std::optional<ash::LobsterImageCandidate>>&
          sanitized_image_candidates);

  // Not owned by this class
  raw_ptr<manta::SnapperProvider> provider_;

  raw_ptr<LobsterCandidateIdGenerator> id_generator_;

  base::WeakPtrFactory<ImageFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_IMAGE_FETCHER_H_
