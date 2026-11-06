// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/semantic_embedder_spaces.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace semantic_embedder {
namespace {

TEST(SemanticEmbedderSpacesTest, SupportsEmbeddingSpaceDerivesFromTable) {
  // Every listed space is supported, and nothing else is.
  const auto spaces = GetSupportedEmbeddingSpaces();
  ASSERT_FALSE(spaces.empty());
  for (const EmbeddingSpaceInfo& space : spaces) {
    EXPECT_TRUE(SupportsEmbeddingSpace(space.name)) << space.name;
  }
  EXPECT_TRUE(SupportsEmbeddingSpace(kEmbeddingGemmaV1Space));
  EXPECT_FALSE(SupportsEmbeddingSpace("other"));
  EXPECT_FALSE(SupportsEmbeddingSpace(""));
}

}  // namespace
}  // namespace semantic_embedder
