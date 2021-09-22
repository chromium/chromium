// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * Test suite for permissions automatic embargo logic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutomaticEmbargoTest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule(true /* useHttpsServer */);

    private static final String GEOLOCATION_TEST_FILE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String NOTIFICATIONS_TEST_FILE =
            "/chrome/test/data/notifications/notification_tester.html";
    private static final String MIDI_TEST_FILE = "/content/test/data/android/midi_permissions.html";
    private static final String MEDIA_TEST_FILE =
            "/content/test/data/android/media_permissions.html";

    private static final int NUMBER_OF_DISMISSALS = 3;

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
    }

    private void runTest(final String testFile, final String javascript, final String updaterPrefix,
            final int nUpdates) throws Exception {
        Tab tab = mPermissionRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter(updaterPrefix, mPermissionRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(updateWaiter));

        for (int i = 0; i < NUMBER_OF_DISMISSALS; ++i) {
            mPermissionRule.setUpUrl(testFile);
            mPermissionRule.runJavaScriptCodeInCurrentTab(javascript);
            PermissionTestRule.waitForDialog(mPermissionRule.getActivity());
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mPermissionRule.getActivity()
                        .getModalDialogManager()
                        .getCurrentPresenterForTest()
                        .dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
            });
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        }

        mPermissionRule.runNoPromptTest(updateWaiter, testFile, javascript, nUpdates, false, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(updateWaiter));
    }

    @Test
    @LargeTest
    @Feature({"Location"})
    @DisableIf.
    Build(message = "Test is failing on Nexus 5X (64-bit) + Android M, see crbug.com/1111001.",
            sdk_is_greater_than = VERSION_CODES.LOLLIPOP_MR1, sdk_is_less_than = VERSION_CODES.N,
            supported_abis_includes = "arm64-v8a")
    public void testGeolocationEmbargo() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        runTest(GEOLOCATION_TEST_FILE, "", "Denied", 0);
    }

    @Test
    @LargeTest
    @Feature({"Notifications"})
    public void testNotificationsEmbargo() throws Exception {
        runTest(NOTIFICATIONS_TEST_FILE, "requestPermission()", "request-callback-denied", 0);
    }

    @Test
    @LargeTest
    @Feature({"MIDI"})
    @FlakyTest(message = "crbug.com/1232946")
    public void testMIDIEmbargo() throws Exception {
        runTest(MIDI_TEST_FILE, "", "fail", 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaPermissions"})
    @DisableIf.Build(message = "Failing on Android P, see crbug.com/1251332.",
            sdk_is_greater_than = VERSION_CODES.O_MR1)
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void
    testCameraEmbargo() throws Exception {
        runTest(MEDIA_TEST_FILE, "initiate_getMicrophone()", "deny", 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaPermissions"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisableIf.Build(message = "Failing on Android P, see crbug.com/1251332.",
            sdk_is_greater_than = VERSION_CODES.O_MR1)
    public void
    testMicrophoneEmbargo() throws Exception {
        runTest(MEDIA_TEST_FILE, "initiate_getCamera()", "deny", 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaPermissions"})
    @DisableIf.Build(message = "Failing on Android P, see crbug.com/1251332.",
            sdk_is_greater_than = VERSION_CODES.O_MR1)
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void
    testMicrophoneAndCameraEmbargo() throws Exception {
        runTest(MEDIA_TEST_FILE, "initiate_getCombined()", "deny", 0);
    }
}
