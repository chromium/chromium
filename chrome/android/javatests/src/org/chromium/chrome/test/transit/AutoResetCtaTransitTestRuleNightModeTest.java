// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.lang.ref.WeakReference;

/** Night mode reuse/relaunch tests for {@link AutoResetCtaTransitTestRule}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
@NullMarked
public class AutoResetCtaTransitTestRuleNightModeTest {
    private static @Nullable WeakReference<ChromeTabbedActivity> sPreviousActivity;
    private static @Nullable Boolean sPreviousNightMode;

    @Rule(order = 0)
    public TestWatcher mResetPreviousOnFailure =
            new TestWatcher() {
                @Override
                protected void failed(Throwable e, Description description) {
                    sPreviousActivity = null;
                    sPreviousNightMode = null;
                }
            };

    @Rule(order = 1)
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @BeforeClass
    public static void setUpClass() {
        sPreviousActivity = null;
        sPreviousNightMode = null;
    }

    @AfterClass
    public static void tearDownClass() {
        sPreviousActivity = null;
        sPreviousNightMode = null;
    }

    @UseMethodParameterBefore(NightModeParams.class)
    public void setUpNightMode(boolean nightModeEnabled) {
        mCtaTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @MediumTest
    public void testA_defaultNightMode() {
        boolean expectedNightMode =
                ChromeNightModeState.isInNightMode(ContextUtils.getApplicationContext());
        verifyNightMode(expectedNightMode);
    }

    @Test
    @MediumTest
    public void testB_explicitDarkMode() {
        mCtaTestRule.setNightModeEnabled(true);
        verifyNightMode(/* expectedNightMode= */ true);
    }

    @Test
    @MediumTest
    public void testC_returnsToDefaultAfterDarkMode() {
        boolean expectedNightMode =
                ChromeNightModeState.isInNightMode(ContextUtils.getApplicationContext());
        verifyNightMode(expectedNightMode);
    }

    @Test
    @MediumTest
    public void testD_ensureActivityInInitialStateAppliesNightMode() {
        ChromeTabbedActivity previousActivity = mCtaTestRule.getActivity();
        boolean targetNightMode = !ChromeNightModeState.isInNightMode(previousActivity);
        mCtaTestRule.setNightModeEnabled(targetNightMode);
        mCtaTestRule.ensureActivityInInitialState();

        ChromeTabbedActivity relaunchedActivity = mCtaTestRule.getActivity();
        assertNotSame(previousActivity, relaunchedActivity);
        assertEquals(targetNightMode, ChromeNightModeState.isInNightMode(relaunchedActivity));

        // startOnBlankPage() should reuse the Activity already launched in the target mode.
        mCtaTestRule.startOnBlankPage();
        assertSame(relaunchedActivity, mCtaTestRule.getActivity());
        sPreviousActivity = new WeakReference<>(relaunchedActivity);
        sPreviousNightMode = targetNightMode;
    }

    @Test
    @MediumTest
    @UseMethodParameter(NightModeParams.class)
    public void testE_firstInNightModeGroup(boolean nightModeEnabled) {
        verifyNightMode(nightModeEnabled);
    }

    @Test
    @MediumTest
    @UseMethodParameter(NightModeParams.class)
    public void testF_reusesActivityInSameNightMode(boolean nightModeEnabled) {
        verifyNightMode(nightModeEnabled);
    }

    private void verifyNightMode(boolean expectedNightMode) {
        mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mCtaTestRule.getActivity();
        assertEquals(expectedNightMode, ChromeNightModeState.isInNightMode(activity));
        if (sPreviousActivity != null && sPreviousNightMode != null) {
            if (sPreviousNightMode == expectedNightMode) {
                assertSame(sPreviousActivity.get(), activity);
            } else {
                assertNotSame(sPreviousActivity.get(), activity);
            }
        }
        sPreviousActivity = new WeakReference<>(activity);
        sPreviousNightMode = expectedNightMode;
    }
}
