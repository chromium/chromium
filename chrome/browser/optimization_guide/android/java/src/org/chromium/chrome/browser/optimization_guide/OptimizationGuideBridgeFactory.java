// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.optimization_guide.proto.HintsProto;

import java.util.ArrayList;
import java.util.List;

/**
 * OptimizationGuideBridge cached by profile.
 */
public class OptimizationGuideBridgeFactory {
    private final ProfileKeyedMap<OptimizationGuideBridge> mProfileToOptimizationGuideBridgeMap =
            ProfileKeyedMap.createMapOfDestroyables();
    private final List<HintsProto.OptimizationType> mOptimizationTypes;

    /**
     * Creates an instance of this class with no observed optimization types.
     */
    public OptimizationGuideBridgeFactory() {
        this(new ArrayList<HintsProto.OptimizationType>());
    }

    /**
     * @param optimizationTypes list of {@link HintsProto.OptimizationType} the {@link
     * OptimizationGuideBridge} is initialized with.
     */
    public OptimizationGuideBridgeFactory(List<HintsProto.OptimizationType> optimizationTypes) {
        mOptimizationTypes = optimizationTypes;
    }

    /**
     * @return {@link OptimizationGuideBridge} for the current last used regular profile
     */
    public OptimizationGuideBridge create() {
        Profile profile = Profile.getLastUsedRegularProfile();
        return mProfileToOptimizationGuideBridgeMap.getForProfile(profile, () -> {
            OptimizationGuideBridge optimizationGuideBridge = new OptimizationGuideBridge();
            if (mOptimizationTypes.size() > 0) {
                optimizationGuideBridge.registerOptimizationTypes(mOptimizationTypes);
            }
            return optimizationGuideBridge;
        });
    }
}
