// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS_PARAM;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.createTabStatesAndMetadataFile;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil.ReturnToChromeBackPressHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.features.start_surface.StartSurfaceTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link ReturnToChromeUtil}. Tests the functionality of return to chrome features that
 * open overview mode if the timeout has passed.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study"
})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@DoNotBatch(reason = "This test suite tests Clank's startup.")
public class ReturnToChromeUtilTest {
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:" + START_SURFACE_RETURN_TIME_SECONDS_PARAM + "/0";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

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

    ReturnToChromeUtil.ReturnToChromeBackPressHandler mBackPressHandler;

    public ReturnToChromeUtilTest() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForAllActivities(
                            new ActivityInflationObserver());
                });
    }

    /**
     * Test that overview mode is triggered in Single-pane non stack tab switcher variation in
     * incognito mode when resuming from incognito mode.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1081754.
    public void testTabSwitcherModeTriggeredWithinThreshold_WarmStart_FromIncognito_NON_V2()
            throws Exception {
        testTabSwitcherModeTriggeredBeyondThreshold();

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                true,
                true);
        Assert.assertTrue(
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals(3, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        // Trigger hide and resume.
        ChromeApplicationTestUtils.fireHomeScreenIntent(
                ApplicationProvider.getApplicationContext());
        mActivityTestRule.resumeMainActivityFromLauncher();

        Assert.assertTrue(
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals(3, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER);
        Assert.assertNotNull(mBackPressHandler.getHandleBackPressChangedSupplier().get());
        Assert.assertFalse(mBackPressHandler.getHandleBackPressChangedSupplier().get());
    }

    /**
     * Ideally we should use {@link StartSurfaceTestUtils#createTabStatesAndMetadataFile} so that we
     * don't need to create tabs with thumbnails and then restart. However, we cannot use stock
     * serialized TabStates like {@link TestTabModelDirectory#M26_GOOGLE_COM} because all of them
     * have URLs that requires network. Serializing URL for EmbeddedTestServer doesn't work because
     * each run might be different. Serializing "about:blank" doesn't work either because when
     * loaded, the URL would somehow become empty string. This issue can also be reproduced by
     * creating tabs with "about:blank" and then restart.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome", "RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "https://crbug.com/1023079, crbug.com/1063984")
    public void testInitialScrollIndex() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        String url = testServer.getURL("/chrome/test/data/android/about.html");

        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 10, 0, url);

        // Trigger thumbnail capturing for the last tab.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(
                mActivityTestRule
                        .getActivity()
                        .getLayoutManager()
                        .isLayoutVisible(LayoutType.TAB_SWITCHER));

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        assertEquals(10, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        assertEquals(9, mActivityTestRule.getActivity().getCurrentTabModel().index());
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.tab_list_recycler_view),
                "10_web_tabs-select_last");
    }

    private void testTabSwitcherModeTriggeredBeyondThreshold() throws Exception {
        createTabStatesAndMetadataFile(new int[] {0, 1});
        startMainActivityWithURLWithoutCurrentTab(null);

        @LayoutType int layoutType = StartSurfaceTestUtils.getStartSurfaceLayoutType();
        if (!mActivityTestRule.getActivity().isTablet()) {
            LayoutTestUtils.waitForLayout(
                    mActivityTestRule.getActivity().getLayoutManager(), layoutType);
        }

        waitTabModelRestoration();
        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        if (!mActivityTestRule.getActivity().isTablet()) {
            LayoutTestUtils.waitForLayout(
                    mActivityTestRule.getActivity().getLayoutManager(), layoutType);
        }
    }

    /**
     * Similar to {@link ChromeTabbedActivityTestRule#startMainActivityWithURL(String url)} but skip
     * verification and tasks regarding current tab.
     */
    private void startMainActivityWithURLWithoutCurrentTab(String url) {
        Intent intent =
                new Intent(TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, url);
        Assert.assertFalse(mInflated.get());
        mActivityTestRule.launchActivity(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        mBackPressHandler =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return new ReturnToChromeBackPressHandler(
                                    mActivityTestRule.getActivity().getActivityTabProvider(),
                                    (shouldHandleTabSwitcherShown) -> {},
                                    mActivityTestRule.getActivity()::getActivityTab,
                                    mActivityTestRule
                                            .getActivity()
                                            .getLayoutStateProviderSupplier(),
                                    false);
                        });
    }

    private void waitTabModelRestoration() {
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }
}
