// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_

#include <string>

namespace contextual_cueing {

enum class ContextualCueingInteraction;

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction,
    const std::string& cuj);

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_METRICS_H_
