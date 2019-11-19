// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests the implementation of {@link ChromeActivity#onGetDirectActions} and
 * {@link ChromeActivity#onPerformDirectAction} and its integration with the
 * {@link DirectActionCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
@TargetApi(24) // For java.util.function.Consumer.
public class DirectActionsInActivityTest {
    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    @Rule
    public DirectActionTestRule mDirectActionRule = new DirectActionTestRule();

    private UserActionTester mActionTester;

    private ChromeActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        if (mActionTester != null) mActionTester.tearDown();
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testDirectActionsDisabled() throws Exception {
        // disableDirectActions() makes AppHooks.createDirectActionCoordinator return null. This
        // should mean that direct actions are not available.
        mDirectActionRule.disableDirectActions();

        mActivityTestRule.startMainActivityOnBlankPage();

        assertThat(DirectActionTestUtils.callOnGetDirectActions(getActivity()), Matchers.empty());
        DirectActionTestUtils.callOnPerformDirectActions(
                getActivity(), "test", (r) -> fail("Unexpected result: " + r));
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCallDirectAction() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Register a single direct action called "test"
            mDirectActionRule.getCoordinator().register(new DirectActionHandler() {
                @Override
                public void reportAvailableDirectActions(DirectActionReporter reporter) {
                    reporter.addDirectAction("test");
                }

                @Override
                public boolean performDirectAction(
                        String actionId, Bundle arguments, Callback<Bundle> callback) {
                    if (!"test".equals(actionId)) return false;

                    Bundle bundle = new Bundle();
                    bundle.putBoolean("ran_test", true);
                    callback.onResult(bundle);
                    return true;
                }
            });
        });

        mActionTester = new UserActionTester();

        assertThat(DirectActionTestUtils.callOnGetDirectActions(getActivity()),
                Matchers.hasItem("test"));
        assertThat(mActionTester.getActions(), Matchers.hasItem("Android.DirectAction.List"));

        HistogramDelta unknownAction = new HistogramDelta(
                "Android.DirectAction.Perform", DirectActionUsageHistogram.DirectActionId.UNKNOWN);
        HistogramDelta otherAction = new HistogramDelta(
                "Android.DirectAction.Perform", DirectActionUsageHistogram.DirectActionId.OTHER);

        DirectActionTestUtils.callOnPerformDirectActions(
                getActivity(), "doesnotexist", (r) -> fail("Unexpected result: " + r));
        assertEquals(1, unknownAction.getDelta());
        assertEquals(0, otherAction.getDelta());

        Bundle result = new Bundle();
        DirectActionTestUtils.callOnPerformDirectActions(
                getActivity(), "test", (r) -> result.putAll((Bundle) r));
        assertThat(result.keySet(), Matchers.contains("ran_test"));
        assertEquals(1, unknownAction.getDelta());
        assertEquals(1, otherAction.getDelta());
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testDirectActionsDisabledInIncognitoMode() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        assertThat(DirectActionTestUtils.callOnGetDirectActions(getActivity()), Matchers.empty());
        DirectActionTestUtils.callOnPerformDirectActions(
                getActivity(), "help", (r) -> fail("Unexpected result: " + r));
    }
}
