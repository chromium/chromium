// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.variations.StudyOuterClass.Study;
import org.chromium.components.variations.StudyOuterClass.Study.Experiment;
import org.chromium.components.variations.StudyOuterClass.Study.Experiment.FeatureAssociation;
import org.chromium.components.variations.VariationsSeedOuterClass.VariationsSeed;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.Date;

/**
 * Tests that seeds saved to disk get loaded correctly on WebView startup.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class VariationsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public boolean needsBrowserProcessStarted() {
            // Don't start the browser process automatically so we can do some setup in the test
            // beforehand.
            return false;
        }
    };

    private void createAndLoadSeedFile(FeatureAssociation features) throws FileNotFoundException {
        // Disable seed verification so we don't reject the fake seed created below.
        VariationsTestUtils.disableSignatureVerificationForTesting();

        // Write a fake seed to disk that will enable a Feature.
        VariationsSeed seed =
                VariationsSeed.newBuilder()
                        .addStudy(Study.newBuilder()
                                          .setName("TestStudy")
                                          .addExperiment(Experiment.newBuilder()
                                                                 .setName("default")
                                                                 .setProbabilityWeight(100)
                                                                 .setFeatureAssociation(features)))
                        .build();
        SeedInfo seedInfo = new SeedInfo();
        seedInfo.signature = "";
        seedInfo.country = "US";
        seedInfo.isGzipCompressed = false;
        seedInfo.date = new Date().getTime();
        seedInfo.seedData = seed.toByteArray();
        FileOutputStream out = new FileOutputStream(VariationsUtils.getNewSeedFile());
        VariationsUtils.writeSeed(out, seedInfo);

        // Because our tests bypass WebView's glue layer, we need to load the seed manually.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            VariationsSeedLoader loader = new VariationsSeedLoader();
            loader.startVariationsInit();
            loader.finishVariationsInit();
        });
    }

    @Test
    @MediumTest
    // This flag forces the variations service to load the seed file from disk rather than using
    // fieldtrial_testing_config.json.
    // TODO(crbug.com/1098037): Reference this via a Java VariationsSwitches class.
    @CommandLineFlags.Add(VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG)
    public void testFeatureEnabled() throws Exception {
        try {
            FeatureAssociation features =
                    FeatureAssociation.newBuilder()
                            .addEnableFeature(VariationsTestUtils.TEST_FEATURE_NAME)
                            .build();
            createAndLoadSeedFile(features);

            // The seed should be loaded during browser process startup.
            mActivityTestRule.startBrowserProcess();

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertTrue("TEST_FEATURE_NAME should be enabled",
                        AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_TEST_FEATURE));
            });
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    // This flag forces the variations service to load the seed file from disk rather than using
    // fieldtrial_testing_config.json.
    // TODO(crbug.com/1098037): Reference this via a Java VariationsSwitches class.
    @CommandLineFlags.Add(VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG)
    public void testSeedFreshnessHistogramWritten() throws Exception {
        String seedFreshnessHistogramName = "Variations.SeedFreshness";
        try {
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(seedFreshnessHistogramName, 0);
            createAndLoadSeedFile(FeatureAssociation.getDefaultInstance());

            // The seed should be loaded during browser process startup.
            mActivityTestRule.startBrowserProcess();

            histogramExpectation.assertExpected(
                    "SeedFreshness should have been written to once, with value 0 (<1 minute)");
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }
}
