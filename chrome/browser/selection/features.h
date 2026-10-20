// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_FEATURES_H_
#define CHROME_BROWSER_SELECTION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace selection {

// Enables requesting smart selection suggestions from the Model Execution
// Service.
BASE_DECLARE_FEATURE(kSmartSelectionServerSuggestions);

// The timeout for requesting smart selection suggestions from the Model
// Execution Service.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSmartSelectionServerTimeout);

// If non-negative, crops the screenshot sent to the Model Execution Service to
// the selected area expanded by a padding of this width in pixels.
BASE_DECLARE_FEATURE_PARAM(int, kSmartSelectionServerCropPadding);

// Whether to include the AnnotatedPageContent in the request sent to the Model
// Execution Service.
BASE_DECLARE_FEATURE_PARAM(bool, kSmartSelectionServerAppendApc);

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_FEATURES_H_
