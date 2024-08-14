// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_candidate_resizer.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/lobster/image_fetcher.h"

LobsterCandidateResizer::LobsterCandidateResizer(ImageFetcher* fetcher)
    : image_fetcher_(fetcher) {}

LobsterCandidateResizer::~LobsterCandidateResizer() = default;

void LobsterCandidateResizer::InflateImage(
    uint32_t seed,
    const std::string& query,
    ash::InflateCandidateCallback callback) {
  if (image_fetcher_ == nullptr) {
    LOG(ERROR) << "No image fetcher found";
    std::move(callback).Run(std::nullopt);
    return;
  }

  image_fetcher_->RequestFullSizeCandidate(
      query, seed,
      base::BindOnce(
          [](ash::InflateCandidateCallback callback,
             const std::vector<ash::LobsterImageCandidate>& image_candidates) {
            if (image_candidates.size() == 0) {
              LOG(ERROR) << "No inflated image found";
              std::move(callback).Run(std::nullopt);
              return;
            }

            std::move(callback).Run(image_candidates[0]);
          },
          std::move(callback)));
}
