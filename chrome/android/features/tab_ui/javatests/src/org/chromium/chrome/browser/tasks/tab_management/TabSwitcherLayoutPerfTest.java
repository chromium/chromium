// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.components.embedder_support.util.UrlConstants.NTP_URL;

import android.os.Build.VERSION_CODES;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.animation.AnimationPerformanceTracker;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the {@link TabSwitcherLayout}, mainly for animation performance. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=" + ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
    "force-fieldtrials=Study/Group"
})
@Restriction({
    UiRestriction.RESTRICTION_TYPE_PHONE,
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
})
@DisableFeatures({ChromeFeatureList.ANDROID_HUB})
public class TabSwitcherLayoutPerfTest {
    private static final String TAG = "SSLayoutPerfTest";
    private static final String BASE_PARAMS =
            "force-fieldtrial-params="
                    + "Study.Group:skip-slow-zooming/false/zooming-min-memory-mb/512";

    /** Flip this to {@code true} to run performance tests locally. */
    private static final boolean PERF_RUN = false;

    // Animations are required for these tests.
    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsTestRule =
            new DisableAnimationsTestRule(true);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;

    private TabSwitcherLayout mTabSwitcherLayout;
    private String mUrl;
    private int mRepeat;
    private long mWaitingTime;
    private int mTabNumCap;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mActivityTestRule.startMainActivityWithURL(NTP_URL);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = TabUiTestHelper.getTabSwitcherLayoutAndVerify(cta);
        mTabSwitcherLayout = (TabSwitcherLayout) layout;
        mUrl = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 1;
        mWaitingTime = 0;
        mTabNumCap = 3;

        if (PERF_RUN) {
            mRepeat = 30;
            // Wait before the animation to get more stable results.
            mWaitingTime = 1000;
            mTabNumCap = 0;
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                                    mActivityTestRule.getActivity()));
                });

        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTab() throws InterruptedException {
        prepareTabs(1, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTab_AndroidAnimation() throws InterruptedException {
        prepareTabs(1, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab using Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10Tabs() throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 10 tabs");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10Tabs_AndroidAnimation() throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 10 tabs using Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1161731",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsWarm() throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 10 tabs (warm)");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1161731",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsWarm_AndroidAnimation()
            throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(
                mUrl, "Tab-to-Grid from live tab with 10 tabs (warm) using Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1184787",
            supported_abis_includes = "x86",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsSoft() throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 10 tabs (soft)");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1184787",
            supported_abis_includes = "x86",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsSoft_AndroidAnimation()
            throws InterruptedException {
        prepareTabs(10, NTP_URL);
        reportTabToGridPerf(
                mUrl, "Tab-to-Grid from live tab with 10 tabs (soft) using Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @DisabledTest(message = "https://crbug.com/1045938")
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsWithoutThumbnail() throws InterruptedException {
        // Note that most of the tabs won't have thumbnails.
        prepareTabs(10, null);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 10 tabs without thumbnails");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @DisabledTest(message = "https://crbug.com/1045938")
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromLiveTabWith10TabsWithoutThumbnail_AndroidAnimation()
            throws InterruptedException {
        // Note that most of the tabs won't have thumbnails.
        prepareTabs(10, null);
        reportTabToGridPerf(
                mUrl,
                "Tab-to-Grid from live tab with 10 tabs without thumbnails using Android"
                        + " animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/1048268")
    public void testTabToGridFromLiveTabWith100Tabs() throws InterruptedException {
        // Skip waiting for loading. Otherwise it would take too long.
        // Note that most of the tabs won't have thumbnails.
        prepareTabs(100, null);
        reportTabToGridPerf(mUrl, "Tab-to-Grid from live tab with 100 tabs without thumbnails");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(1, NTP_URL);
        reportTabToGridPerf(NTP_URL, "Tab-to-Grid from NTP");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "crbug.com/1484339")
    public void testTabToGridFromNtp_AndroidAnimation() throws InterruptedException {
        prepareTabs(1, NTP_URL);
        reportTabToGridPerf(NTP_URL, "Tab-to-Grid from NTP using Android animation");
    }

    /**
     * Make Chrome have {@code numTabs} or Tabs with {@code url} loaded.
     *
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *     be saved.
     */
    private void prepareTabs(int numTabs, @Nullable String url) throws InterruptedException {
        assertTrue(numTabs >= 1);
        assertEquals(1, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        // Only run the full size when doing local perf tests.
        if (mTabNumCap > 0) numTabs = Math.min(numTabs, mTabNumCap);

        if (url != null) mActivityTestRule.loadUrl(url);
        for (int i = 0; i < numTabs - 1; i++) {
            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    R.id.new_tab_menu_id);
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            if (url != null) mActivityTestRule.loadUrl(url);

            Tab previousTab =
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .getTabAt(i);

            boolean fixPendingReadbacks =
                    mActivityTestRule
                                    .getActivity()
                                    .getTabContentManager()
                                    .getInFlightCapturesForTesting()
                            != 0;

            // When there are pending readbacks due to detached Tabs, try to fix it by switching
            // back to that tab.
            if (fixPendingReadbacks) {
                int lastIndex = i;
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .setIndex(lastIndex, TabSelectionType.FROM_USER, false));
            }
            checkThumbnailsExist(previousTab);
            if (fixPendingReadbacks) {
                int currentIndex = i + 1;
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .setIndex(currentIndex, TabSelectionType.FROM_USER, false));
            }
        }
        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(),
                null,
                null,
                WAIT_TIMEOUT_SECONDS * 10);
        assertEquals(
                numTabs, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getActivity()
                                    .getTabContentManager()
                                    .getInFlightCapturesForTesting(),
                            Matchers.is(0));
                });
    }

    private void reportTabToGridPerf(String fromUrl, String description)
            throws InterruptedException {
        List<Float> frameRates = new LinkedList<>();
        List<Float> frameInterval = new LinkedList<>();
        int[] iteration = new int[] {0};
        AnimationPerformanceTracker.Listener collector =
                (metrics) -> {
                    assertThat(
                            "Elapsed time was less than expected duration for iteration "
                                    + iteration[0],
                            metrics.getElapsedTimeMs(),
                            greaterThanOrEqualTo(
                                    (long)
                                            Math.floor(
                                                    TabSwitcherLayout.ZOOMING_DURATION
                                                            * CompositorAnimator.sDurationScale)));
                    frameRates.add(metrics.getFramesPerSecond());
                    frameInterval.add((float) metrics.getMaxFrameIntervalMs());
                };

        mActivityTestRule.loadUrl(fromUrl);
        Thread.sleep(mWaitingTime);

        for (int i = 0; i < mRepeat; i++) {
            iteration[0] = i;
            addPerfListenerForTesting(collector);
            Thread.sleep(mWaitingTime);
            TestThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mActivityTestRule
                                    .getActivity()
                                    .getLayoutManager()
                                    .showLayout(LayoutType.TAB_SWITCHER, true));
            Thread.sleep(mWaitingTime);
            final int expectedSize = i + 1;
            CriteriaHelper.pollInstrumentationThread(
                    () -> frameRates.size() == expectedSize,
                    "Have not got PerfListener callback",
                    DEFAULT_MAX_TIME_TO_POLL * 10,
                    DEFAULT_POLLING_INTERVAL);
            assertTrue(
                    mActivityTestRule
                            .getActivity()
                            .getLayoutManager()
                            .isLayoutVisible(LayoutType.TAB_SWITCHER));

            removePerfListenerForTesting(collector);
            TabUiTestHelper.pressBackOnTabSwitcher(mTabSwitcherLayout);
            LayoutTestUtils.waitForLayout(
                    mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
            Thread.sleep(mWaitingTime);
        }
        assertEquals(mRepeat, frameRates.size());
        Log.i(
                TAG,
                "%s: fps = %.2f, maxFrameInterval = %.0f",
                description,
                median(frameRates),
                median(frameInterval));
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "https://crbug.com/1184787 and https://crbug.com/1363755")
    public void testGridToTabToCurrentNtp() throws InterruptedException, TimeoutException {
        prepareTabs(1, NTP_URL);
        reportGridToTabPerf(false, false, "Grid-to-Tab to current NTP");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "https://crbug.com/1184787 and https://crbug.com/1363755")
    public void testGridToTabToCurrentNtp_AndroidAnimation()
            throws InterruptedException, TimeoutException {
        prepareTabs(1, NTP_URL);
        reportGridToTabPerf(false, false, "Grid-to-Tab to current NTP Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "crbug.com/1087608")
    public void testGridToTabToOtherNtp() throws InterruptedException, TimeoutException {
        prepareTabs(2, NTP_URL);
        reportGridToTabPerf(true, false, "Grid-to-Tab to other NTP");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "crbug.com/1087608")
    public void testGridToTabToOtherNtp_AndroidAnimation()
            throws InterruptedException, TimeoutException {
        prepareTabs(2, NTP_URL);
        reportGridToTabPerf(true, false, "Grid-to-Tab to other NTP Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "crbug.com/1087608")
    public void testGridToTabToCurrentLive() throws InterruptedException, TimeoutException {
        prepareTabs(1, mUrl);
        reportGridToTabPerf(false, false, "Grid-to-Tab to current live tab");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "crbug.com/1087608")
    public void testGridToTabToCurrentLive_AndroidAnimation()
            throws InterruptedException, TimeoutException {
        prepareTabs(1, mUrl);
        reportGridToTabPerf(false, false, "Grid-to-Tab to current live tab Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "https://crbug.com/1225926")
    public void testGridToTabToOtherLive() throws InterruptedException, TimeoutException {
        prepareTabs(2, mUrl);
        reportGridToTabPerf(true, false, "Grid-to-Tab to other live tab");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisabledTest(message = "https://crbug.com/1225926")
    public void testGridToTabToOtherLive_AndroidAnimation()
            throws InterruptedException, TimeoutException {
        prepareTabs(2, mUrl);
        reportGridToTabPerf(true, false, "Grid-to-Tab to other live tab Android animation");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1161731",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        prepareTabs(2, mUrl);
        reportGridToTabPerf(true, true, "Grid-to-Tab to other frozen tab");
    }

    @Test
    @EnormousTest
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS)
    @DisableIf.Build(
            message = "Flaky on Android P, see https://crbug.com/1161731",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q)
    public void testGridToTabToOtherFrozen_AndroidAnimation() throws InterruptedException {
        prepareTabs(2, mUrl);
        reportGridToTabPerf(true, true, "Grid-to-Tab to other frozen tab Android animation");
    }

    private void reportGridToTabPerf(
            boolean switchToAnotherTab, boolean killBeforeSwitching, String description)
            throws InterruptedException {
        List<Float> frameRates = new LinkedList<>();
        List<Float> frameInterval = new LinkedList<>();
        int[] iteration = new int[] {0};
        AnimationPerformanceTracker.Listener collector =
                (metrics) -> {
                    assertThat(
                            "Elapsed time was less than expected duration for iteration "
                                    + iteration[0],
                            metrics.getElapsedTimeMs(),
                            greaterThanOrEqualTo(
                                    (long)
                                            Math.floor(
                                                    TabSwitcherLayout.ZOOMING_DURATION
                                                            * CompositorAnimator.sDurationScale)));
                    frameRates.add(metrics.getFramesPerSecond());
                    frameInterval.add((float) metrics.getMaxFrameIntervalMs());
                };
        Thread.sleep(mWaitingTime);

        for (int i = 0; i < mRepeat; i++) {
            iteration[0] = i;
            removePerfListenerForTesting(collector);
            LayoutTestUtils.startShowingAndWaitForLayout(
                    mActivityTestRule.getActivity().getLayoutManager(),
                    LayoutType.TAB_SWITCHER,
                    true);

            int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
            final int targetIndex = switchToAnotherTab ? 1 - index : index;
            Tab targetTab =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(targetIndex);
            if (killBeforeSwitching) {
                WebContentsUtils.simulateRendererKilled(targetTab.getWebContents());
                Thread.sleep(1000);
            }

            addPerfListenerForTesting(collector);
            Thread.sleep(mWaitingTime);
            Espresso.onView(
                            allOf(
                                    isDescendantOfA(
                                            withId(
                                                    TabUiTestHelper.getTabSwitcherAncestorId(
                                                            mActivityTestRule.getActivity()))),
                                    withId(R.id.tab_list_recycler_view)))
                    .perform(
                            RecyclerViewActions.actionOnItemAtPosition(
                                    targetIndex, ViewActions.click()));

            final int expectedSize = i + 1;
            CriteriaHelper.pollInstrumentationThread(
                    () -> frameRates.size() == expectedSize,
                    "Have not got PerfListener callback",
                    DEFAULT_MAX_TIME_TO_POLL * 10,
                    DEFAULT_POLLING_INTERVAL);
            LayoutTestUtils.waitForLayout(
                    mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
            Thread.sleep(1000);
        }
        assertEquals(mRepeat, frameRates.size());
        Log.i(
                TAG,
                "%s: fps = %.2f, maxFrameInterval = %.0f",
                description,
                median(frameRates),
                median(frameInterval));
    }

    private void checkThumbnailsExist(Tab tab) {
        File etc1File = TabContentManager.getTabThumbnailFileEtc1(tab);
        CriteriaHelper.pollInstrumentationThread(
                etc1File::exists,
                "The thumbnail " + etc1File.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);

        File jpegFile = TabContentManager.getTabThumbnailFileJpeg(tab.getId());
        CriteriaHelper.pollInstrumentationThread(
                jpegFile::exists,
                "The thumbnail " + jpegFile.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);
    }

    private float median(List<Float> list) {
        float[] array = new float[list.size()];
        for (int i = 0; i < array.length; i++) {
            array[i] = list.get(i);
        }
        Arrays.sort(array);
        return array[array.length / 2];
    }

    private void addPerfListenerForTesting(AnimationPerformanceTracker.Listener perfListener) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                        mTabSwitcherLayout.addPerfListenerForTesting(perfListener);
                });
    }

    private void removePerfListenerForTesting(AnimationPerformanceTracker.Listener perfListener) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                        mTabSwitcherLayout.removePerfListenerForTesting(perfListener);
                });
    }
}
