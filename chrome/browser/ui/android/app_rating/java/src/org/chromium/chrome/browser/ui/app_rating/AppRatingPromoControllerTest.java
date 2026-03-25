// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.SegmentationPlatformConstants;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;

/** Unit tests for {@link AppRatingPromoController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppRatingPromoControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SegmentationPlatformService mSegmentationService;
    @Captor private ArgumentCaptor<Callback<ClassificationResult>> mCallbackCapturer;

    private Activity mActivity;
    private AppRatingPromoController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        SegmentationPlatformServiceFactory.setForTests(mSegmentationService);
        mController = new AppRatingPromoController(mProfile, mActivity);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_APP_RATING_PROMPT)
    public void testMaybeShowPromo_FeatureDisabled() {
        mController.maybeShowPromo();
        verify(mSegmentationService, never()).getClassificationResult(any(), any(), any(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_RATING_PROMPT)
    public void testMaybeShowPromo_QueriesSegmentation() {
        mController.maybeShowPromo();
        verify(mSegmentationService)
                .getClassificationResult(
                        eq(SegmentationPlatformConstants.POWER_USER_KEY),
                        any(PredictionOptions.class),
                        any(),
                        any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_RATING_PROMPT)
    public void testOnSegmentationResultReceived_HighEngagement() {
        mController.maybeShowPromo();
        verify(mSegmentationService)
                .getClassificationResult(any(), any(), any(), mCallbackCapturer.capture());

        ClassificationResult result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {SegmentationPlatformConstants.SEARCH_USER_MODEL_LABEL_HIGH},
                        0L);

        // This should not crash and should proceed to trigger the (currently logged) success path.
        mCallbackCapturer.getValue().onResult(result);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_RATING_PROMPT)
    public void testOnSegmentationResultReceived_LowEngagement() {
        mController.maybeShowPromo();
        verify(mSegmentationService)
                .getClassificationResult(any(), any(), any(), mCallbackCapturer.capture());

        ClassificationResult result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {SegmentationPlatformConstants.SEARCH_USER_MODEL_LABEL_LOW},
                        0L);

        mCallbackCapturer.getValue().onResult(result);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_RATING_PROMPT)
    public void testOnSegmentationResultReceived_ActivityDestroyed() {
        mController.maybeShowPromo();
        verify(mSegmentationService)
                .getClassificationResult(any(), any(), any(), mCallbackCapturer.capture());

        mActivity.finish();

        ClassificationResult result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {SegmentationPlatformConstants.SEARCH_USER_MODEL_LABEL_HIGH},
                        0L);

        // Callback should abort safely because activity is finishing.
        mCallbackCapturer.getValue().onResult(result);
    }
}
