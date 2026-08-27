// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.FieldTrialList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.components.variations.LayerOuterClass.Layer;
import org.chromium.components.variations.LayerOuterClass.Layer.EntropyMode;
import org.chromium.components.variations.LayerOuterClass.Layer.LayerMember;
import org.chromium.components.variations.LayerOuterClass.Layer.LayerMember.SlotRange;
import org.chromium.components.variations.LayerOuterClass.LayerMemberReference;
import org.chromium.components.variations.StudyOuterClass.Study;
import org.chromium.components.variations.StudyOuterClass.Study.ActivationType;
import org.chromium.components.variations.StudyOuterClass.Study.Channel;
import org.chromium.components.variations.StudyOuterClass.Study.Experiment;
import org.chromium.components.variations.StudyOuterClass.Study.Experiment.FeatureAssociation;
import org.chromium.components.variations.StudyOuterClass.Study.Filter;
import org.chromium.components.variations.StudyOuterClass.Study.Platform;
import org.chromium.components.variations.VariationsSeedOuterClass.VariationsSeed;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.Date;

/** Tests that seeds saved to disk get loaded correctly on WebView startup. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class VariationsTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public VariationsTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        // Don't start the browser process automatically so we can do some setup in
                        // the test beforehand.
                        return false;
                    }
                };
    }

    private void createAndLoadSeedFile(VariationsSeed seed) throws FileNotFoundException {
        // Disable seed verification so we don't reject the fake seed created below.
        VariationsTestUtils.disableSignatureVerificationForTesting();

        // Write a fake seed to disk.
        SeedInfo seedInfo = new SeedInfo();
        seedInfo.signature = "";
        seedInfo.country = "US";
        seedInfo.isGzipCompressed = false;
        seedInfo.date = new Date().getTime();
        seedInfo.seedData = seed.toByteArray();
        FileOutputStream out = new FileOutputStream(VariationsUtils.getNewSeedFile());
        VariationsUtils.writeSeed(
                out,
                seedInfo,
                /* lowEntropySource= */ -1,
                /* limitedEntropyRandomizationSource= */ null);

        // Because our tests bypass WebView's glue layer, we need to load the seed manually.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VariationsSeedLoader loader = new VariationsSeedLoader();
                    loader.startVariationsInit();
                    loader.finishVariationsInit();
                });
    }

    @Test
    @MediumTest
    // This flag forces the variations service to load the seed file from disk rather than using
    // fieldtrial_testing_config.json.
    @CommandLineFlags.Add(VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG)
    public void testFeatureEnabled() throws Exception {
        try {
            Experiment launchGroup =
                    Experiment.newBuilder()
                            .setName("Launched")
                            .setProbabilityWeight(100)
                            .setFeatureAssociation(
                                    FeatureAssociation.newBuilder()
                                            .addEnableFeature(AwFeatures.WEBVIEW_TEST_FEATURE))
                            .build();
            VariationsSeed seed =
                    VariationsSeed.newBuilder()
                            .addStudy(
                                    Study.newBuilder()
                                            .setName("TestStudy")
                                            .addExperiment(launchGroup))
                            .build();
            WebViewCachedFlags.initForTesting(new InMemorySharedPreferences());
            createAndLoadSeedFile(seed);

            // The seed should be loaded during browser process startup.
            mActivityTestRule.startBrowserProcess();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertTrue(
                                "TEST_FEATURE_NAME should be enabled",
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
    @CommandLineFlags.Add(VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG)
    public void testSeedFreshnessHistogramWritten() throws Exception {
        String seedFreshnessHistogramName = "Variations.SeedFreshness";
        try {
            Experiment defaultGroup =
                    Experiment.newBuilder()
                            .setName("default")
                            .setProbabilityWeight(100)
                            .setFeatureAssociation(FeatureAssociation.getDefaultInstance())
                            .build();
            VariationsSeed seed =
                    VariationsSeed.newBuilder()
                            .addStudy(
                                    Study.newBuilder()
                                            .setName("TestStudy")
                                            .addExperiment(defaultGroup))
                            .build();
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(seedFreshnessHistogramName, 0);
            WebViewCachedFlags.initForTesting(new InMemorySharedPreferences());
            createAndLoadSeedFile(seed);

            // The seed should be loaded during browser process startup.
            mActivityTestRule.startBrowserProcess();

            histogramExpectation.assertExpected(
                    "SeedFreshness should have been written to once, with value 0 (<1 minute)");
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    // Provides test coverage for some VariationsSeed validation logic related to entropy. See
    // SeedHasMisconfiguredEntropy().
    //
    // A study can use entropy if it has permanent consistency and a weighted group with an
    // experiment ID. Right now, only low entropy can be used on Android WebView. Studies that use
    // limited entropy are not yet allowed on Android WebView.
    @CommandLineFlags.Add(VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG)
    public void testEntropyConsumingStudies() throws Exception {
        String limitedStudyName = "LimitedLayerConstrainedStudy";
        String lowStudyName = "LowLayerConstrainedStudy";
        String layerlessStudyName = "LayerlessStudy";

        try {
            Layer limitedLayer =
                    Layer.newBuilder()
                            .setId(1)
                            .setNumSlots(100)
                            .setEntropyMode(EntropyMode.LIMITED)
                            .addMembers(
                                    LayerMember.newBuilder()
                                            .setId(1)
                                            .addSlots(
                                                    SlotRange.newBuilder().setStart(0).setEnd(99)))
                            .build();

            Layer lowLayer =
                    Layer.newBuilder()
                            .setId(2)
                            .setNumSlots(100)
                            .setEntropyMode(EntropyMode.LOW)
                            .addMembers(
                                    LayerMember.newBuilder()
                                            .setId(1)
                                            .addSlots(
                                                    SlotRange.newBuilder().setStart(0).setEnd(99)))
                            .build();

            Filter filter =
                    Filter.newBuilder()
                            .addChannel(Channel.CANARY)
                            .addChannel(Channel.DEV)
                            .addChannel(Channel.BETA)
                            .addChannel(Channel.STABLE)
                            .addChannel(Channel.UNKNOWN)
                            .addPlatform(Platform.PLATFORM_ANDROID_WEBVIEW)
                            .build();

            Study limitedStudy =
                    Study.newBuilder()
                            .setName(limitedStudyName)
                            .setActivationType(ActivationType.ACTIVATE_ON_STARTUP)
                            .setFilter(filter)
                            .setLayer(
                                    LayerMemberReference.newBuilder()
                                            .setLayerId(1)
                                            .addLayerMemberIds(1))
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group1")
                                            .setProbabilityWeight(1)
                                            .setGoogleWebExperimentId(10001))
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group2")
                                            .setProbabilityWeight(1)
                                            .setGoogleWebExperimentId(10002))
                            .build();

            Study lowStudy =
                    Study.newBuilder()
                            .setName(lowStudyName)
                            .setActivationType(ActivationType.ACTIVATE_ON_STARTUP)
                            .setFilter(filter)
                            .setLayer(
                                    LayerMemberReference.newBuilder()
                                            .setLayerId(2)
                                            .addLayerMemberIds(1))
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group1")
                                            .setProbabilityWeight(1)
                                            .setGoogleWebExperimentId(20001))
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group2")
                                            .setProbabilityWeight(1)
                                            .setGoogleWebExperimentId(20002))
                            .build();

            Study layerlessStudy =
                    Study.newBuilder()
                            .setName(layerlessStudyName)
                            .setActivationType(ActivationType.ACTIVATE_ON_STARTUP)
                            .setFilter(filter)
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group1")
                                            .setProbabilityWeight(50)
                                            .setGoogleWebExperimentId(30001))
                            .addExperiment(
                                    Experiment.newBuilder()
                                            .setName("Group2")
                                            .setProbabilityWeight(50)
                                            .setGoogleWebExperimentId(30002))
                            .build();

            VariationsSeed seed =
                    VariationsSeed.newBuilder()
                            .addLayers(limitedLayer)
                            .addLayers(lowLayer)
                            .addStudy(limitedStudy)
                            .addStudy(lowStudy)
                            .addStudy(layerlessStudy)
                            .build();

            WebViewCachedFlags.initForTesting(new InMemorySharedPreferences());
            createAndLoadSeedFile(seed);

            // The seed should be loaded during browser process startup.
            mActivityTestRule.startBrowserProcess();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // TODO(crbug.com/532511229): Update the first assertion once the client
                        // supports limited entropy randomization.
                        Assert.assertFalse(
                                "Limited-layer constrained study should not exist",
                                FieldTrialList.trialExists(limitedStudyName));
                        Assert.assertTrue(
                                "Low-layer constrained study should exist",
                                FieldTrialList.trialExists(lowStudyName));
                        Assert.assertTrue(
                                "Layerless study should exist",
                                FieldTrialList.trialExists(layerlessStudyName));
                    });
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }
}
