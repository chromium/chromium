// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS_PARAM;

import android.app.Activity;
import android.content.Intent;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CommandLine;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.features.start_surface.InstantStartTest;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests the functionality of return to chrome features that open overview mode if the timeout
 * has passed.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "force-fieldtrials=Study/Group"})
public class ReturnToChromeTest {
    // clang-format on
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("NoInstant"),
                    new ParameterSet().value(true).name("Instant"));

    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    class ActivityInflationObserver implements ActivityStateListener, InflationObserver {
        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.CREATED && activity instanceof ChromeTabbedActivity) {
                ((ChromeTabbedActivity) activity).getLifecycleDispatcher().register(this);
            }
        }

        @Override
        public void onPreInflationStartup() {}

        @Override
        public void onPostInflationStartup() {
            mInflated.set(true);
        }
    }
    private final AtomicBoolean mInflated = new AtomicBoolean();

    private final boolean mUseInstantStart;

    public ReturnToChromeTest(boolean useInstantStart) {
        mUseInstantStart = useInstantStart;
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);
        if (mUseInstantStart) {
            CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        }
        ApplicationStatus.registerStateListenerForAllActivities(new ActivityInflationObserver());
    }

    /**
     * Test that overview mode is not triggered if the delay is longer than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/100000"
            + "/start_surface_variation/single/open_ntp_instead_of_start/true"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1081754.
    @DisabledTest(message="https://crbug.com/1130696")
    public void testTabSwitcherModeNotTriggeredWithinThreshold() throws Exception {
        // clang-format on
        InstantStartTest.createTabStateFile(new int[] {0, 1});
        startMainActivityWithURLWithoutCurrentTab(null);

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse(ReturnToChromeExperimentsUtil
                                                      .shouldShowStartSurfaceAsTheHomePage()));

        Assert.assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        waitTabModelRestoration();
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT));
        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        Assert.assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    /**
     * Test that with {@link StartSurfaceConfiguration#START_SURFACE_OPEN_NTP_INSTEAD_OF_START}
     * variation, overview mode is not triggered in Single-pane variation with NTP intent, even
     * though the delay is longer than the interval between stop and start. Plus, a NTP should be
     * created.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/100000"
            + "/start_surface_variation/single/open_ntp_instead_of_start/true"})
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1) // See https://crbug.com/1091268.
    @DisabledTest(message="https://crbug.com/1144184")
    public void testTabSwitcherModeNotTriggeredWithinThreshold_NTP() throws Exception {
        // clang-format on
        InstantStartTest.createTabStateFile(new int[] {0, 1});
        startMainActivityWithURLWithoutCurrentTab(UrlConstants.NTP_URL);

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse(ReturnToChromeExperimentsUtil
                                                      .shouldShowStartSurfaceAsTheHomePage()));

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertFalse(
                    mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        waitTabModelRestoration();

        // 3 tabs since we created NTP in this case.
        assertEquals(3, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertFalse(
                    mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    /**
     * Test that overview mode is triggered in Single-pane variation with no tabs, even though
     * the delay is longer than the interval between stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/100000"
            + "/start_surface_variation/single/open_ntp_instead_of_start/true"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1081754.
    public void testTabSwitcherModeTriggeredWithinThreshold_NoTab() {
        // clang-format on
        startMainActivityWithURLWithoutCurrentTab(null);

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(ReturnToChromeExperimentsUtil
                                                     .shouldShowStartSurfaceAsTheHomePageNoTabs()));

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        waitTabModelRestoration();
        assertEquals(0, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    /**
     * Test that overview mode is triggered in Single-pane variation with NTP intent, even though
     * the delay is longer than the interval between stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/100000"
            + "/start_surface_variation/single"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1081754.
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1) // See https://crbug.com/1091268.
    public void testTabSwitcherModeTriggeredWithinThreshold_NTP() throws Exception {
        // clang-format on
        InstantStartTest.createTabStateFile(new int[] {0, 1});
        startMainActivityWithURLWithoutCurrentTab(UrlConstants.NTP_URL);

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(ReturnToChromeExperimentsUtil
                                                     .shouldShowStartSurfaceAsTheHomePage()));

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        waitTabModelRestoration();
        // Not 3 because we don't create a tab for NTP in this case.
        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    /**
     * Test that overview mode is triggered in Single-pane non stack tab switcher variation in
     * incognito mode when resuming from incognito mode.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/single/"
            + "show_last_active_tab_only/true/open_ntp_instead_of_start/true"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1081754.
    public void testTabSwitcherModeTriggeredWithinThreshold_WarmStart_FromIncognito_NON_V2() throws Exception {
        // clang-format on

        // TODO(crbug.com/1095637): Make it work for instant start.
        assumeFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        testTabSwitcherModeTriggeredBeyondThreshold();

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), true, true);
        Assert.assertTrue(
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals(3, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        // Trigger hide and resume.
        ChromeApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        mActivityTestRule.resumeMainActivityFromLauncher();

        Assert.assertTrue(
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals(3, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @DisabledTest(message = "https://crbug.com/1130696")
    public void testTabSwitcherModeTriggeredBeyondThreshold() throws Exception {
        // clang-format on
        InstantStartTest.createTabStateFile(new int[] {0, 1});
        startMainActivityWithURLWithoutCurrentTab(null);

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        waitTabModelRestoration();
        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @FlakyTest(message = "crbug.com/1040896")
    public void testTabSwitcherModeTriggeredBeyondThreshold_UMA() throws Exception {
        // clang-format on
        testTabSwitcherModeTriggeredBeyondThreshold();

        assertThat(mActivityTestRule.getActivity().isTablet()).isFalse();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT),
                    Matchers.is(1));
        });
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(true)));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(true)
                        + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                mActivityTestRule.getActivity()
                                        .getTabModelSelector()
                                        .getTotalTabCount())));
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start.
     */
    @Test
    @MediumTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.Q, sdk_is_greater_than = VERSION_CODES.O,
            message = "crbug.com/1134361")
    @DisabledTest(message = "https://crbug.com/1130696")
    public void testTabSwitcherModeTriggeredBeyondThreshold_WarmStart() throws Exception {
        // clang-format on
        testTabSwitcherModeTriggeredBeyondThreshold();

        // Redo to trigger warm startup UMA.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA.
     */
    @Test
    @MediumTest
    @Feature({"ReturnToChrome"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @FlakyTest(message = "crbug.com/1040896")
    public void testTabSwitcherModeTriggeredBeyondThreshold_WarmStart_UMA() throws Exception {
        // clang-format on
        testTabSwitcherModeTriggeredBeyondThreshold_WarmStart();

        assertThat(mActivityTestRule.getActivity().isTablet()).isFalse();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT),
                    Matchers.is(2));
        });
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(false)));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(false)
                        + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                mActivityTestRule.getActivity()
                                        .getTabModelSelector()
                                        .getTotalTabCount())));
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    public void testTabSwitcherModeTriggeredBeyondThreshold_NoTabs() {
        // clang-format on
        // Cannot use ChromeTabbedActivityTestRule.startMainActivityFromLauncher() because
        // there's no tab.
        startMainActivityWithURLWithoutCurrentTab(null);

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        waitTabModelRestoration();
        assertEquals(0, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA for the no-tab condition.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @DisabledTest(message = "http://crbug.com/1027315")
    public void testTabSwitcherModeTriggeredBeyondThreshold_NoTabs_UMA() {
        // clang-format on
        testTabSwitcherModeTriggeredBeyondThreshold_NoTabs();

        assertThat(mActivityTestRule.getActivity().isTablet()).isFalse();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT),
                    Matchers.is(1));
        });
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(true)));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + ReturnToChromeExperimentsUtil.coldStartBucketName(true)
                        + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                mActivityTestRule.getActivity()
                                        .getTabModelSelector()
                                        .getTotalTabCount())));
    }

    /**
     * Ideally we should use {@link InstantStartTest#createTabStateFile} so that we don't need to
     * create tabs with thumbnails and then restart. However, we cannot use stock serialized
     * TabStates like {@link TestTabModelDirectory#M26_GOOGLE_COM} because all of them have URLs
     * that requires network. Serializing URL for EmbeddedTestServer doesn't work because each run
     * might be different. Serializing "about:blank" doesn't work either because when loaded, the
     * URL would somehow become empty string. This issue can also be reproduced by creating tabs
     * with "about:blank" and then restart.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome", "RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0"
            + "/start_surface_variation/omniboxonly"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "https://crbug.com/1063984")
    public void testInitialScrollIndex() throws Exception {
        // clang-format on
        // Instant start is not applicable since we need to create tabs and restart.
        assumeTrue(!mUseInstantStart);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String url = testServer.getURL("/chrome/test/data/android/about.html");

        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 10, 0, url);

        // Trigger thumbnail capturing for the last tab.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        assertEquals(10, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        assertEquals(9, mActivityTestRule.getActivity().getCurrentTabModel().index());
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(
                                       org.chromium.chrome.tab_ui.R.id.tab_list_view),
                "10_web_tabs-select_last");
    }

    /**
     * Similar to {@link ChromeTabbedActivityTestRule#startMainActivityWithURL(String url)}
     * but skip verification and tasks regarding current tab.
     */
    private void startMainActivityWithURLWithoutCurrentTab(String url) {
        Intent intent =
                new Intent(TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, url);
        Assert.assertFalse(mInflated.get());
        mActivityTestRule.launchActivity(intent);
        if (mUseInstantStart) {
            CriteriaHelper.pollUiThread(mInflated::get);
        } else {
            mActivityTestRule.waitForActivityNativeInitializationComplete();
        }
    }

    private void waitTabModelRestoration() {
        if (mUseInstantStart) {
            Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

            CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .startDelayedNativeInitializationForTests());
        }
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }
}
