// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.res.Configuration;
import android.os.Build;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Test tablet / phone layout change. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class tests activity restart behavior and thus cannot be batched.")
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // See crbug.com/1302618.
public class TabletPhoneLayoutChangeTest {
    private static final long TIMEOUT_MS = 10000;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/407729913")
    public void testIsRecreatedOnLayoutChange() throws TimeoutException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        boolean isTestOnTablet = cta.isTablet();
        CallbackHelper helper = new CallbackHelper();
        Configuration config = cta.getSavedConfigurationForTesting();

        // Pretend the device is in another mode.
        config.smallestScreenWidthDp =
                DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + (isTestOnTablet ? -1 : 1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getLifecycleDispatcher()
                            .register((RecreateObserver) helper::notifyCalled);
                    Assert.assertTrue(
                            "Activity should be ready for tablet mode change.",
                            cta.getTabletMode().changed);
                    cta.getDisplayAndroidObserverForTesting().onCurrentModeChanged(null);
                    Assert.assertTrue(
                            "ChromeActivity#mIsRecreatingForTabletModeChange should be true.",
                            cta.recreatingForTabletModeChangeForTesting());
                });

        helper.waitForOnly("Activity should be restart");
        Configuration newConfig = cta.getResources().getConfiguration();
        config = cta.getSavedConfigurationForTesting();
        Assert.assertEquals(
                "Saved config should be updated after recreate.",
                newConfig.smallestScreenWidthDp,
                config.smallestScreenWidthDp);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.Q) // See crbug.com/404979701.
    public void testUrlBarStateRetention() throws TimeoutException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ToolbarManager toolbarManager = cta.getToolbarManager();
        String urlBarText = "hello";

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        toolbarManager.setUrlBarFocusAndText(
                                true, OmniboxFocusReason.OMNIBOX_TAP, urlBarText));

        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isOmniboxFocused = toolbarManager.isUrlBarFocused();
                    Criteria.checkThat(isOmniboxFocused, Matchers.is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        simulateTabletModeChange();
        ChromeTabbedActivity newCta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean doesOmniboxStateMatch =
                            newCta.getToolbarManager() != null
                                    && newCta.getToolbarManager().isUrlBarFocused()
                                    && urlBarText.equals(
                                            newCta.getToolbarManager()
                                                    .getUrlBarTextWithoutAutocomplete());
                    Criteria.checkThat(doesOmniboxStateMatch, Matchers.is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.Q) // See crbug.com/404979701.
    public void testTabSwitcherStateRetention() throws TimeoutException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.findViewById(R.id.tab_switcher_button).performClick();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isTabSwitcherShown =
                            cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER);
                    Criteria.checkThat(isTabSwitcherShown, Matchers.is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        simulateTabletModeChange();
        ChromeTabbedActivity newCta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isTabSwitcherShown =
                            newCta.getCompositorViewHolderSupplier().get() != null
                                    && newCta.getLayoutManager() != null
                                    && newCta.getLayoutManager()
                                            .isLayoutVisible(LayoutType.TAB_SWITCHER);
                    Criteria.checkThat(isTabSwitcherShown, Matchers.is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void simulateTabletModeChange() throws TimeoutException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        boolean isTestOnTablet = cta.isTablet();
        CallbackHelper helper = new CallbackHelper();
        Configuration config = cta.getSavedConfigurationForTesting();
        Configuration newConfig = new Configuration(config);
        config.smallestScreenWidthDp =
                DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + (isTestOnTablet ? -1 : 1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getLifecycleDispatcher()
                            .register((DestroyObserver) helper::notifyCalled);
                    Assert.assertTrue(
                            "Activity should be ready for tablet mode change.",
                            cta.getTabletMode().changed);
                    cta.performOnConfigurationChanged(newConfig);
                    Assert.assertTrue(
                            "ChromeActivity#mIsRecreatingForTabletModeChange should be true.",
                            cta.recreatingForTabletModeChangeForTesting());
                });
        mActivityTestRule.recreateActivity();
        helper.waitForOnly("Wait for old Activity being destroyed.");
    }
}
