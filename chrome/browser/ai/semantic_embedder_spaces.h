// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_SEMANTIC_EMBEDDER_SPACES_H_
#define CHROME_BROWSER_AI_SEMANTIC_EMBEDDER_SPACES_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/time/time.h"

// The embedding spaces the Semantic Embedder serves.
namespace semantic_embedder {

// One embedding space this browser serves.
struct EmbeddingSpaceInfo {
  std::string_view name;
  // When the browser may stop serving this space; null if not scheduled.
  std::optional<base::Time> shutdown_date;
};

// The embedding space served by the EmbeddingGemma v1 model.
inline constexpr char kEmbeddingGemmaV1Space[] = "embeddinggemma-300m";

// Every space this browser serves.
base::span<const EmbeddingSpaceInfo> GetSupportedEmbeddingSpaces();

// True if `embedding_space` is in GetSupportedEmbeddingSpaces(). The result
// must not change while the browser runs, because
// AIManager::CreateSemanticEmbedder() reports an unsupported space as a bad
// message.
bool SupportsEmbeddingSpace(std::string_view embedding_space);

}  // namespace semantic_embedder

#endif  // CHROME_BROWSER_AI_SEMANTIC_EMBEDDER_SPACES_H_
