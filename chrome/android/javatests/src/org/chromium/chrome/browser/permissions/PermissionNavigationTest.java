// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.modaldialog.ChromeTabModalPresenter;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.TestAndroidPermissionDelegate;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.permissions.DismissalType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/** Test suite for interaction between permissions requests and navigation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PermissionNavigationTest {
    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String TEST_FILE = "/content/test/data/android/permission_navigation.html";

    private static final String DISMISS_TYPE_HISTOGRAM =
            "Permissions.Prompt.Geolocation.ModalDialog.Dismissed.Method";

    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;

    public PermissionNavigationTest() {}

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();

        // Some bots on continuous integration may have the system-level location setting off, in
        // which case the permission request would be auto-denied as it will not have a user
        // gesture. See: GeolocationPermissionContextAndroid::CanShowLocationSettingsDialog().
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
    }

    /**
     * Check that modal permission prompts and queued permission requests are removed upon
     * navigation.
     */
    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testNavigationDismissesModalPermissionPrompt() throws Exception {
        mPermissionRule.setUpUrl(TEST_FILE);
        mPermissionRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        mPermissionRule.waitForDialogShownState(true);

        Tab tab = mPermissionRule.getActivity().getActivityTab();
        final CallbackHelper callbackHelper = new CallbackHelper();
        EmptyTabObserver navigationWaiter =
                new EmptyTabObserver() {
                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        callbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(navigationWaiter));

        mPermissionRule.runJavaScriptCodeInCurrentTab("navigate()");

        callbackHelper.waitForCallback(0);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(navigationWaiter));

        mPermissionRule.waitForDialogShownState(false);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testUmaMetricsForDismissalReasonsNavigateBackAndTouchOutsideTheScrim()
            throws Exception {
        mPermissionRule.setUpUrl(TEST_FILE);
        mPermissionRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        mPermissionRule.waitForDialogShownState(true);

        // Verify dismissing by pressing back is recorded in UMA
        var histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(DISMISS_TYPE_HISTOGRAM, DismissalType.NAVIGATE_BACK)
                        .build();

        PermissionTestRule.waitForDialog(mPermissionRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPermissionRule.getActivity().onBackPressed();
                });

        histogramExpectation.assertExpected(
                "Should record tapping back to dismiss permission prompt in UMA");

        // Verify touching outside the scrim is recorded in UMA
        mPermissionRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        mPermissionRule.waitForDialogShownState(true);

        histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(DISMISS_TYPE_HISTOGRAM, DismissalType.TOUCH_OUTSIDE)
                        .build();
        PermissionTestRule.waitForDialog(mPermissionRule.getActivity());

        ChromeTabModalPresenter mTabModalPresenter =
                (ChromeTabModalPresenter)
                        mPermissionRule
                                .getActivity()
                                .getModalDialogManager()
                                .getPresenterForTest(ModalDialogType.TAB);

        View dialogContainerForTest = mTabModalPresenter.getDialogContainerForTest();
        ThreadUtils.runOnUiThreadBlocking(dialogContainerForTest::performClick);
        histogramExpectation.assertExpected(
                "Should record tapping outside the scrim to dismiss permission prompt in UMA");
    }
}
