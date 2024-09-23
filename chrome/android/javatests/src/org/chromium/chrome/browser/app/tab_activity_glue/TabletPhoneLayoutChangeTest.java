// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.res.Configuration;
import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Test tablet / phone layout change. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class tests activity restart behavior and thus cannot be batched.")
public class TabletPhoneLayoutChangeTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) // See crbug.com/1302618.
    @DisabledTest(message = "crbug.com/1444252")
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

                    // Simulate invocation of #onSaveInstanceState to verify that the saved instance
                    // state contains DID_CHANGE_TABLET_MODE.
                    Bundle outState = new Bundle();
                    cta.onSaveInstanceState(outState);
                    Assert.assertTrue(
                            "DID_CHANGE_TABLET_MODE in the saved instance state should be true.",
                            outState.getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE));
                });

        helper.waitForOnly("Activity should be restart");
        Configuration newConfig = cta.getResources().getConfiguration();
        config = cta.getSavedConfigurationForTesting();
        Assert.assertEquals(
                "Saved config should be updated after recreate.",
                newConfig.smallestScreenWidthDp,
                config.smallestScreenWidthDp);
    }
}
