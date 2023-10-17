// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.hasSize;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.Constants;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class SegmentationPlatformServiceFactoryTest {
    @Rule public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private CallbackHelper mCallbackHelper = new CallbackHelper();

    @Test
    @MediumTest
    public void testGetClassificationResult_withNullInputContext() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        SegmentationPlatformService segmentationPlatformService =
                                SegmentationPlatformServiceFactory.getForProfile(
                                        Profile.getLastUsedRegularProfile());

                        PredictionOptions options = new PredictionOptions(true);
                        segmentationPlatformService.getClassificationResult(
                                "intentional_user",
                                options,
                                null,
                                new Callback<ClassificationResult>() {
                                    @Override
                                    public void onResult(ClassificationResult result) {
                                        Assert.assertEquals(
                                                PredictionStatus.NOT_READY, result.status);
                                        assertThat(
                                                result.orderedLabels,
                                                org.hamcrest.Matchers.empty());
                                        mCallbackHelper.notifyCalled();
                                    }
                                });
                    }
                });

        mCallbackHelper.waitForNext();
    }

    @Test
    @MediumTest
    public void testGetClassificationResult_withOnDemandModel() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        SegmentationPlatformService segmentationPlatformService =
                                SegmentationPlatformServiceFactory.getForProfile(
                                        Profile.getLastUsedRegularProfile());

                        PredictionOptions options = new PredictionOptions(true);
                        InputContext inputContext = new InputContext();
                        inputContext.addEntry(
                                Constants.CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING_INPUT,
                                ProcessedValue.fromFloat(1.0f));
                        inputContext.addEntry(
                                Constants.CONTEXTUAL_PAGE_ACTIONS_READER_MODE_INPUT,
                                ProcessedValue.fromFloat(0.0f));
                        inputContext.addEntry("url", ProcessedValue.fromGURL(GURL.emptyGURL()));

                        segmentationPlatformService.getClassificationResult(
                                "contextual_page_actions",
                                options,
                                inputContext,
                                new Callback<ClassificationResult>() {
                                    @Override
                                    public void onResult(ClassificationResult result) {
                                        Assert.assertEquals(
                                                PredictionStatus.SUCCEEDED, result.status);
                                        assertThat(result.orderedLabels, hasSize(1));
                                        assertThat(
                                                result.orderedLabels, contains("price_tracking"));
                                        mCallbackHelper.notifyCalled();
                                    }
                                });
                    }
                });

        mCallbackHelper.waitForNext();
    }
}
