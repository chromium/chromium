// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.UiDisableIf;

import java.util.concurrent.TimeoutException;

/**
 * Tests that cached permissions for Trusted Web Activities have an effect on the actual permission
 * state.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// See: https://crbug.com/1120707
@DisableIf.Device(type = {UiDisableIf.TABLET})
@Batch(PER_CLASS)
public class TrustedWebActivityPermissionsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final int NOTIFICATIONS = ContentSettingsType.NOTIFICATIONS;
    private static final int GEOLOCATION = ContentSettingsType.GEOLOCATION;

    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private Origin mOrigin;
    private String mPackage;
    private TrustedWebActivityPermissionManager mPermissionManager;

    @Before
    public void setUp() throws TimeoutException {
        mCustomTabActivityTestRule.setFinishActivity(true);
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

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

        mPermissionManager = ChromeApplicationImpl.getComponent().resolveTwaPermissionManager();
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, NOTIFICATIONS, true));
        assertEquals("\"granted\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void blockNotifications() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, NOTIFICATIONS, false));
        assertEquals("\"denied\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void unregisterTwa() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, NOTIFICATIONS, true));
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
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, NOTIFICATIONS, true));
        assertTrue(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mPermissionManager.unregister(mOrigin); });
        assertFalse(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));
    }

    @Test
    @SmallTest
    public void allowGeolocation() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, GEOLOCATION, true));
        assertTrue(WebappRegistry.getInstance()
                           .getTrustedWebActivityPermissionStore()
                           .arePermissionEnabled(GEOLOCATION, mOrigin));
    }

    @Test
    @SmallTest
    public void blockGeolocation() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionManager.updatePermission(mOrigin, mPackage, GEOLOCATION, false));
        assertFalse(WebappRegistry.getInstance()
                            .getTrustedWebActivityPermissionStore()
                            .arePermissionEnabled(GEOLOCATION, mOrigin));
    }

    private String getNotificationPermission() throws TimeoutException {
        return mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab("Notification.permission");
    }
}
