// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_image_provider_from_memory.h"

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr gfx::Size kPreviewImageSize = gfx::Size(512, 512);
constexpr gfx::Size kFullImageSize = gfx::Size(1024, 1024);
constexpr base::TimeDelta kMultipleImagesFetchDelay = base::Seconds(3);
constexpr base::TimeDelta kSingleImageFetchDelay = base::Seconds(1);

const SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorMAGENTA);
  return bitmap;
}

const std::string GetTestJpgBytes(const SkBitmap& bitmap) {
  std::optional<std::vector<uint8_t>> encoded_data =
      gfx::JPEGCodec::Encode(bitmap, /*quality=*/50);

  return std::string(base::as_string_view(encoded_data.value()));
}

ash::LobsterResult CreateTestingLobsterResult(
    LobsterCandidateIdGenerator* id_generator,
    const std::string& user_query,
    const std::string& rewritten_query,
    int num_candidates,
    const gfx::Size& image_dimensions) {
  CHECK(id_generator);

  std::vector<ash::LobsterImageCandidate> image_candidates;
  for (int index = 0; index < num_candidates; ++index) {
    image_candidates.push_back(ash::LobsterImageCandidate(
        id_generator->GenerateNextId(),
        std::string(GetTestJpgBytes(CreateTestBitmap(
            image_dimensions.width(), image_dimensions.height()))),
        static_cast<uint32_t>(index), user_query, rewritten_query));
  }
  return image_candidates;
}

}  // namespace

LobsterImageProviderFromMemory::LobsterImageProviderFromMemory(
    LobsterCandidateIdGenerator* id_generator)
    : id_generator_(id_generator) {}

LobsterImageProviderFromMemory::~LobsterImageProviderFromMemory() = default;

void LobsterImageProviderFromMemory::RequestMultipleCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  ash::LobsterResult results = CreateTestingLobsterResult(
      id_generator_, /*user_query=*/query,
      /*rewritten_query=*/base::StrCat({"rewritten: ", query}), num_candidates,
      kPreviewImageSize);
  delay_timer_.Start(FROM_HERE, kMultipleImagesFetchDelay,
                     base::BindOnce(
                         [](const ash::LobsterResult results,
                            ash::RequestCandidatesCallback result_callback) {
                           std::move(result_callback).Run(results);
                         },
                         results, std::move(callback)));
}

void LobsterImageProviderFromMemory::RequestSingleCandidateWithSeed(
    const std::string& query,
    uint32_t seed,
    ash::RequestCandidatesCallback callback) {
  ash::LobsterResult results = CreateTestingLobsterResult(
      id_generator_, /*user_query=*/query,
      /*rewritten_query=*/base::StrCat({"rewritten: ", query}),
      /*num_candidates=*/1, kFullImageSize);
  delay_timer_.Start(FROM_HERE, kSingleImageFetchDelay,
                     base::BindOnce(
                         [](const ash::LobsterResult results,
                            ash::RequestCandidatesCallback result_callback) {
                           std::move(result_callback).Run(results);
                         },
                         results, std::move(callback)));
}
