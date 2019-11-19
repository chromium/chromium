// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.background_sync.BackgroundSyncPwaDetector;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;

/**
 * Tests that cached permissions for Trusted Web Activities have an effect on the actual permission
 * state.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityPermissionsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private Origin mOrigin;
    private String mPackage;
    private TrustedWebActivityPermissionManager mPermissionManager;

    @Before
    public void setUp() throws TimeoutException {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        // TWAs only work with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mOrigin = Origin.create(mTestPage);
        mPackage = InstrumentationRegistry.getTargetContext().getPackageName();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), mTestPage));

        mPermissionManager = ChromeApplication.getComponent().resolveTwaPermissionManager();
        mPermissionManager.clearForTesting();
        assertEquals("\"default\"", getNotificationPermission());
    }

    @After
    public void tearDown() {
        mPermissionManager.clearForTesting();
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void allowNotifications() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() ->
                mPermissionManager.register(mOrigin, mPackage, true));
        assertEquals("\"granted\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void blockNotifications() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() ->
                mPermissionManager.register(mOrigin, mPackage, false));
        assertEquals("\"denied\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void unregisterTwa() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() ->
                mPermissionManager.register(mOrigin, mPackage, true));
        assertEquals("\"granted\"", getNotificationPermission());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPermissionManager.unregister(mOrigin);
        });
        assertEquals("\"default\"", getNotificationPermission());
    }

    @Test
    @SmallTest
    public void detectTwa() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.register(mOrigin, mPackage, true));
        assertTrue(BackgroundSyncPwaDetector.isTwaInstalled(mOrigin.toString()));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mPermissionManager.unregister(mOrigin); });
        assertFalse(BackgroundSyncPwaDetector.isTwaInstalled(mOrigin.toString()));
    }

    private String getNotificationPermission() throws TimeoutException {
        return mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab("Notification.permission");
    }
}
