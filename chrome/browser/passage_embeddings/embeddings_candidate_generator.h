// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDINGS_CANDIDATE_GENERATOR_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDINGS_CANDIDATE_GENERATOR_H_

#include <string>
#include <vector>

#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace passage_embeddings {

// Generates candidates for embeddings from `apc. Will generate at most
// `page_content_passages_to_generate` for the page content passage type.
std::vector<std::pair<std::string, PassageType>> GenerateEmbeddingsCandidates(
    const optimization_guide::proto::AnnotatedPageContent& apc,
    int page_content_passages_to_generate);

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDINGS_CANDIDATE_GENERATOR_H_
