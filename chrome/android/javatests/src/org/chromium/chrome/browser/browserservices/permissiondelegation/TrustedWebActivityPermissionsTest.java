// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/**
 * Tests that cached permissions for Trusted Web Activities have an effect on the actual permission
 * state.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// See: https://crbug.com/1120707
@DisableIf.Device(DeviceFormFactor.TABLET)
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

    @Before
    public void setUp() throws TimeoutException {
        mCustomTabActivityTestRule.setFinishActivity(true);
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        // TWAs only work with HTTPS.
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mOrigin = Origin.create(mTestPage);
        mPackage = ApplicationProvider.getApplicationContext().getPackageName();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage));

        WebappRegistry.getInstance().getPermissionStore().clearForTesting();
        assertEquals("\"default\"", getNotificationPermission());
    }

    @After
    public void tearDown() {
        WebappRegistry.getInstance().getPermissionStore().clearForTesting();
    }

    @Test
    @MediumTest
    public void allowNotifications() throws TimeoutException {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSettingValues.ALLOW));
        assertEquals("\"granted\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void blockNotifications() throws TimeoutException {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSettingValues.BLOCK));
        assertEquals("\"denied\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void unregisterTwa() throws TimeoutException {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSettingValues.ALLOW));
        assertEquals("\"granted\"", getNotificationPermission());

        runOnUiThreadBlocking(() -> InstalledWebappPermissionManager.unregister(mOrigin));
        assertEquals("\"default\"", getNotificationPermission());
    }

    @Test
    @SmallTest
    public void detectTwa() {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSettingValues.ALLOW));
        assertTrue(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));

        runOnUiThreadBlocking(() -> InstalledWebappPermissionManager.unregister(mOrigin));
        assertFalse(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));
    }

    @Test
    @SmallTest
    public void allowGeolocation() {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, GEOLOCATION, ContentSettingValues.ALLOW));
        assertEquals(
                Integer.valueOf(ContentSettingValues.ALLOW),
                WebappRegistry.getInstance()
                        .getPermissionStore()
                        .getPermission(GEOLOCATION, mOrigin));
    }

    @Test
    @SmallTest
    public void blockGeolocation() {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, GEOLOCATION, ContentSettingValues.BLOCK));
        assertEquals(
                Integer.valueOf(ContentSettingValues.BLOCK),
                WebappRegistry.getInstance()
                        .getPermissionStore()
                        .getPermission(GEOLOCATION, mOrigin));
    }

    private String getNotificationPermission() throws TimeoutException {
        return mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab("Notification.permission");
    }
}
