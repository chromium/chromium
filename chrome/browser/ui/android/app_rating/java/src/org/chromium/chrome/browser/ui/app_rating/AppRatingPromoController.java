// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.SegmentationPlatformConstants;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.ref.WeakReference;

/**
 * Controller for coordinating the app rating promotion based on user engagement. This class handles
 * the logic for determining if a user is highly engaged using the Segmentation Platform before
 * triggering the Play Store review flow.
 */
@NullMarked
public class AppRatingPromoController {
    private final Profile mProfile;
    private final WeakReference<Activity> mActivityRef;

    public AppRatingPromoController(Profile profile, Activity activity) {
        mProfile = profile;
        mActivityRef = new WeakReference<>(activity);
    }

    /** Entry point to potentially trigger the app rating prompt. */
    public void maybeShowPromo() {
        if (!ChromeFeatureList.sAndroidAppRatingPrompt.isEnabled()
                || !UserPrefs.areNativePrefsLoaded(mProfile)) {
            return;
        }

        // Ensure the prompt is only shown once
        if (UserPrefs.get(mProfile).getBoolean(Pref.APP_RATING_PROMPT_SHOWN)) {
            return;
        }

        // TODO(crbug.com/493342419): Implement a 72-hour Clank-wide cooldown check to ensure
        // that no other promotional UI has been shown recently.

        checkSegmentationResult();
    }

    /** Queries the Segmentation Platform for the user's engagement level. */
    private void checkSegmentationResult() {
        SegmentationPlatformService segmentationService =
                SegmentationPlatformServiceFactory.getForProfile(mProfile);

        // We use cached results to keep startup impact minimal.
        PredictionOptions predictionOptions = new PredictionOptions(true);

        segmentationService.getClassificationResult(
                SegmentationPlatformConstants.POWER_USER_KEY,
                predictionOptions,
                new InputContext(),
                this::onSegmentationResultReceived);
    }

    /** Evaluates the engagement classification result and triggers the prompt if eligible. */
    private void onSegmentationResultReceived(ClassificationResult result) {
        Activity activity = mActivityRef.get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return;
        }

        // Verify that the user meets the high engagement "power user" criteria.
        // The Segmentation Platform returns categorical labels in descending order of
        // relevance. We only check index 0 as it represents the highest probability
        // classification for the user.
        if (result.status != PredictionStatus.SUCCEEDED
                || result.orderedLabels.isEmpty()
                || !SegmentationPlatformConstants.SEARCH_USER_MODEL_LABEL_HIGH.equals(
                        result.orderedLabels.get(0))) {
            return;
        }

        triggerAppRatingReviewFlow(activity);
    }

    private void triggerAppRatingReviewFlow(Activity activity) {
        // The Play Store In-App Review API is a black box that fails silently if the user has
        // already rated the app or seen the prompt too recently.
        // It does not inform us if the UI was actually shown.
        // To strictly avoid spamming users, we record the attempt as a success.
        UserPrefs.get(mProfile).setBoolean(Pref.APP_RATING_PROMPT_SHOWN, true);
        AppRatingManager manager = AppRatingManagerFactory.create();
        manager.requestAndShowReviewFlow(
                activity,
                () -> {
                    // This callback only indicates that the API flow has finished (or failed
                    // silently). It does NOT mean the user saw the dialog or provided a rating.
                    // TODO(crbug.com/493340627): Log or update metrics */
                });
    }
}
