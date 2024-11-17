// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;

import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;

public class OptimizationGuideDecisionWithMetadata {

    private final @OptimizationGuideDecision int mDecision;
    @Nullable private final Any mMetadata;

    OptimizationGuideDecisionWithMetadata(
            @OptimizationGuideDecision int decision, @Nullable Any metadata) {
        mDecision = decision;
        mMetadata = metadata;
    }

    public int getDecision() {
        return mDecision;
    }

    @Nullable
    public Any getMetadata() {
        return mMetadata;
    }
}
