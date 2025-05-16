// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_SNAPPER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_SNAPPER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/ash/lobster/lobster_image_fetcher.h"
#include "components/manta/proto/manta.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace manta {

namespace proto {
class Response;
}  // namespace proto

class SnapperProvider;
struct MantaStatus;
}  // namespace manta

class LobsterImageProviderFromSnapper : public LobsterImageFetcher::Provider {
 public:
  LobsterImageProviderFromSnapper(manta::SnapperProvider* provider,
                                  LobsterCandidateIdGenerator* id_generator);
  ~LobsterImageProviderFromSnapper() override;

  void RequestMultipleCandidates(const std::string& query,
                                 int num_candidates,
                                 ash::RequestCandidatesCallback) override;
  void RequestSingleCandidateWithSeed(const std::string& query,
                                      uint32_t seed,
                                      ash::RequestCandidatesCallback) override;

 private:
  void OnCandidatesRequested(const std::string& query,
                             ash::RequestCandidatesCallback,
                             std::unique_ptr<manta::proto::Response> response,
                             manta::MantaStatus status);
  void OnImagesSanitized(
      const ::google::protobuf::RepeatedPtrField<::manta::proto::FilteredData>&
          filtered_data,
      ash::RequestCandidatesCallback callback,
      const std::vector<std::optional<ash::LobsterImageCandidate>>&
          sanitized_image_candidates);

  // Not owned by this class
  raw_ptr<manta::SnapperProvider> provider_;
  raw_ptr<LobsterCandidateIdGenerator> id_generator_;
  base::WeakPtrFactory<LobsterImageProviderFromSnapper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_IMAGE_PROVIDER_FROM_SNAPPER_H_
