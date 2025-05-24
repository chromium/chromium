// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"

namespace contextual_cueing {

MockContextualCueingService::MockContextualCueingService()
    : ContextualCueingService(
          /*page_content_extraction_service=*/nullptr,
          /*optimization_guide_keyed_service=*/nullptr,
          /*loading_predictor=*/nullptr,
          /*pref_service=*/nullptr,
          /*template_url_service=*/nullptr) {}

MockContextualCueingService::~MockContextualCueingService() = default;

}  // namespace contextual_cueing
