// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_FEATURES_H_
#define CHROME_BROWSER_SELECTION_FEATURES_H_

#include "base/feature_list.h"

namespace selection {

// Enables requesting smart selection suggestions from the Model Execution
// Service.
BASE_DECLARE_FEATURE(kSmartSelectionServerSuggestions);

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_FEATURES_H_
