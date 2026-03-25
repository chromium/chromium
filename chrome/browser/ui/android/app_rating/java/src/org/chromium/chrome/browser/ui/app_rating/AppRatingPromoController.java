// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.SegmentationPlatformConstants;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;

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
        if (!ChromeFeatureList.sAndroidAppRatingPrompt.isEnabled()) {
            return;
        }

        // TODO(crbug.com/493340627): Check the syncable user lifetime preference to ensure
        // the prompt is only shown once across all devices.

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
        if (result.status != PredictionStatus.SUCCEEDED
                || result.orderedLabels.isEmpty()
                || !SegmentationPlatformConstants.SEARCH_USER_MODEL_LABEL_HIGH.equals(
                        result.orderedLabels.get(0))) {
            return;
        }

        // TODO(crbug.com/493342419): Trigger the Play Store Review flow using AppRatingManager.
    }
}
