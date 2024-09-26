// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

/**
 * Test suite for Geo-Location functionality.
 *
 * <p>These tests rely on the device being specially setup (which the bots do automatically): -
 * Global location is enabled. - Google location is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GeolocationTest {
    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";

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
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(updateWaiter));
        mPermissionRule.runAllowTest(
                updateWaiter, TEST_FILE, javascript, nUpdates, withGesture, isDialog);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(updateWaiter));
    }

    /** Verify Geolocation creates a dialog and receives a mock location. */
    @Test
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialog() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, true, true);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location when dialogs are enabled and
     * there is no user gesture.
     */
    @Test
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialogNoGesture() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, true);
    }

    /** Verify Geolocation creates a dialog and receives multiple locations. */
    @Test
    @MediumTest
    @Feature({"Location"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R, message = "crbug.com/362792693")
    public void testGeolocationWatchDialog() throws Exception {
        runTest("initiate_watchPosition()", 2, true, true);
    }
}
