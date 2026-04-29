// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_MODEL_EXECUTION_LOG_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_MODEL_EXECUTION_LOG_H_

// Convenience macro for emitting OPTIMIZATION_GUIDE_LOGs where
// optimization_keyed_service_ is defined.
#define MODEL_EXECUTION_LOG(message)                                     \
  if (optimization_guide_keyed_service_) {                               \
    OPTIMIZATION_GUIDE_LOG(                                              \
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,    \
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(), \
        (message));                                                      \
  }

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_MODEL_EXECUTION_LOG_H_
