// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;

@NullMarked
public class OptimizationGuideDecisionWithMetadata {

    private final @OptimizationGuideDecision int mDecision;
    private final @Nullable Any mMetadata;

    OptimizationGuideDecisionWithMetadata(
            @OptimizationGuideDecision int decision, @Nullable Any metadata) {
        mDecision = decision;
        mMetadata = metadata;
    }

    public int getDecision() {
        return mDecision;
    }

    public @Nullable Any getMetadata() {
        return mMetadata;
    }
}
