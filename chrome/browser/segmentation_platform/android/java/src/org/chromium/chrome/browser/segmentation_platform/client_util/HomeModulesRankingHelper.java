// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.segmentation_platform.client_util;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;

import java.util.List;

/** Helper class to fetch module order. */
@NullMarked
public final class HomeModulesRankingHelper {
    /**
     * Fetches the module rank including both stable and ephemeral modules.
     *
     * @param profile the profile to fetch the module
     * @param freshnessAndEphemeralInputs the input context for the request with all signals for
     *     modules
     * @param callback the callback to be called when the module rank is fetched
     */
    public static void fetchModulesRank(
            Profile profile,
            InputContext freshnessAndEphemeralInputs,
            Callback<List<String>> callback) {
        HomeModulesRankingHelperJni.get()
                .getClassificationResult(
                        profile,
                        createPredictionOptions(),
                        /* inputContext= */ freshnessAndEphemeralInputs,
                        result -> {
                            assert result.status == PredictionStatus.SUCCEEDED;
                            callback.onResult(result.orderedLabels);
                        });
    }

    /**
     * Notifies the module ranker that the card has been interacted with.
     *
     * @param profile the profile to notify the module ranker
     * @param moduleLabel the module label to notify the module ranker
     */
    public static void notifyCardInteracted(Profile profile, String moduleLabel) {
        HomeModulesRankingHelperJni.get().notifyCardInteracted(profile, moduleLabel);
    }

    /**
     * Notifies the module ranker that the card has been shown.
     *
     * @param profile the profile to notify the module ranker
     * @param moduleLabel the module label to notify the module ranker
     */
    public static void notifyCardShown(Profile profile, String moduleLabel) {
        HomeModulesRankingHelperJni.get().notifyCardShown(profile, moduleLabel);
    }

    /**
     * Creates an instance of PredictionOptions. If feature flag is enabled generate ondemand
     * prediction options else will generate cache prediction options.
     */
    private static PredictionOptions createPredictionOptions() {
        boolean usePredictionOptions =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2);
        if (usePredictionOptions) {
            return new PredictionOptions(
                    /* onDemandExecution= */ true,
                    /* canUpdateCacheForFutureRequests= */ true,
                    /* fallbackAllowed= */ true);
        } else {
            return new PredictionOptions(/* onDemandExecution= */ false);
        }
    }

    @NativeMethods
    public interface Natives {
        void getClassificationResult(
                @JniType("Profile*") Profile profile,
                PredictionOptions predictionOptions,
                InputContext inputContext,
                Callback<ClassificationResult> callback);

        void notifyCardShown(@JniType("Profile*") Profile profile, String cardLabel);

        void notifyCardInteracted(@JniType("Profile*") Profile profile, String cardLabel);
    }
}
