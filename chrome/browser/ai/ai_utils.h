// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_UTILS_H_
#define CHROME_BROWSER_AI_AI_UTILS_H_

#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

class AIUtils {
 public:
  using LanguageCodes =
      std::optional<std::vector<blink::mojom::AILanguageCodePtr>>;

  static blink::mojom::ModelStreamingResponseStatus ConvertModelExecutionError(
      optimization_guide::OptimizationGuideModelExecutionError::
          ModelExecutionError error);

  static constexpr int kNormalizedDownloadProgressMax = 0x10000;
};

#endif  // CHROME_BROWSER_AI_AI_UTILS_H_
