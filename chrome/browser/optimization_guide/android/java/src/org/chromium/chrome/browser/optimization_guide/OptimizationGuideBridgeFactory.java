// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.optimization_guide.proto.HintsProto;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * OptimizationGuideBridge cached by profile.
 */
public class OptimizationGuideBridgeFactory {
    @VisibleForTesting
    protected final Map<Profile, OptimizationGuideBridge> mProfileToOptimizationGuideBridgeMap =
            new HashMap<>();
    private final List<HintsProto.OptimizationType> mOptimizationTypes;
    private ProfileManager.Observer mProfileManagerObserver;

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
        if (mProfileManagerObserver == null) {
            mProfileManagerObserver = new ProfileManager.Observer() {
                @Override
                public void onProfileAdded(Profile profile) {}

                @Override
                public void onProfileDestroyed(Profile destroyedProfile) {
                    if (mProfileToOptimizationGuideBridgeMap.containsKey(destroyedProfile)) {
                        mProfileToOptimizationGuideBridgeMap.get(destroyedProfile).destroy();
                        mProfileToOptimizationGuideBridgeMap.remove(destroyedProfile);
                    }
                }
            };
            ProfileManager.addObserver(mProfileManagerObserver);
        }
    }

    /**
     * @return {@link OptimizationGuideBridge} for the current last used regular profile
     */
    public OptimizationGuideBridge create() {
        Profile profile = Profile.getLastUsedRegularProfile();
        OptimizationGuideBridge optimizationGuideBridge =
                mProfileToOptimizationGuideBridgeMap.get(profile);
        if (optimizationGuideBridge == null) {
            optimizationGuideBridge = new OptimizationGuideBridge();
            if (mOptimizationTypes.size() > 0) {
                optimizationGuideBridge.registerOptimizationTypes(mOptimizationTypes);
            }
            mProfileToOptimizationGuideBridgeMap.put(profile, optimizationGuideBridge);
        }
        return optimizationGuideBridge;
    }
}
