// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.DialogShownCriteria;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Test suite for interaction between permissions requests and navigation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class PermissionNavigationTest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String TEST_FILE = "/content/test/data/android/permission_navigation.html";

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
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testNavigationDismissesModalPermissionPrompt() throws Exception {
        mPermissionRule.setUpUrl(TEST_FILE);
        mPermissionRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        DialogShownCriteria criteriaShown = new DialogShownCriteria(
                mPermissionRule.getActivity().getModalDialogManager(), "Dialog not shown", true);
        CriteriaHelper.pollUiThread(criteriaShown);

        mPermissionRule.runJavaScriptCodeInCurrentTab("navigate()");

        Tab tab = mPermissionRule.getActivity().getActivityTab();
        final CallbackHelper callbackHelper = new CallbackHelper();
        EmptyTabObserver navigationWaiter = new EmptyTabObserver() {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                callbackHelper.notifyCalled();
            }
        };
        tab.addObserver(navigationWaiter);
        callbackHelper.waitForCallback(0);
        tab.removeObserver(navigationWaiter);

        DialogShownCriteria criteriaNotShown = new DialogShownCriteria(
                mPermissionRule.getActivity().getModalDialogManager(), "Dialog shown", false);
        CriteriaHelper.pollUiThread(criteriaNotShown);
    }
}
