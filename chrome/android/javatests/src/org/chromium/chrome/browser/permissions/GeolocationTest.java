// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

/**
 * Test suite for Geo-Location functionality.
 *
 * These tests rely on the device being specially setup (which the bots do automatically):
 * - Global location is enabled.
 * - Google location is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class GeolocationTest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";
    private static final String PERSIST_ACCEPT_HISTOGRAM =
            "Permissions.Prompt.Accepted.Persisted.Geolocation";

    public GeolocationTest() {}

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
    }

    private void runTest(String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        Tab tab = mPermissionRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter("Count:", mPermissionRule.getActivity());
        tab.addObserver(updateWaiter);
        mPermissionRule.runAllowTest(
                updateWaiter, TEST_FILE, javascript, nUpdates, withGesture, isDialog);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives a mock location.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Location", "Main"})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testGeolocationPlumbingAllowedInfoBar() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, false);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialog() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, true, true);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location when dialogs are
     * enabled and there is no user gesture.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialogNoGesture() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, true);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives multiple locations.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Location"})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testGeolocationWatchInfoBar() throws Exception {
        runTest("initiate_watchPosition()", 2, false, false);
    }

    /**
     * Verify Geolocation creates a dialog and receives multiple locations.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"Location"})
    public void testGeolocationWatchDialog() throws Exception {
        runTest("initiate_watchPosition()", 2, true, true);
    }
}
