// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/semantic_embedder_spaces.h"

#include <algorithm>
#include <array>

namespace semantic_embedder {

namespace {

// Every space this browser serves.
constexpr auto kSupportedEmbeddingSpaces = std::to_array<EmbeddingSpaceInfo>({
    {kEmbeddingGemmaV1Space},
});

}  // namespace

base::span<const EmbeddingSpaceInfo> GetSupportedEmbeddingSpaces() {
  return kSupportedEmbeddingSpaces;
}

bool SupportsEmbeddingSpace(std::string_view embedding_space) {
  return std::ranges::any_of(GetSupportedEmbeddingSpaces(),
                             [&](const EmbeddingSpaceInfo& space) {
                               return space.name == embedding_space;
                             });
}

}  // namespace semantic_embedder
