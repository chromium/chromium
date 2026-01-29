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
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests that cached permissions for Trusted Web Activities have an effect on the actual permission
 * state.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// See: https://crbug.com/1120707
@DisableIf.Device(DeviceFormFactor.ONLY_TABLET)
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
                                mOrigin, mPackage, NOTIFICATIONS, ContentSetting.ALLOW));
        assertEquals("\"granted\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void blockNotifications() throws TimeoutException {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSetting.BLOCK));
        assertEquals("\"denied\"", getNotificationPermission());
    }

    @Test
    @MediumTest
    public void unregisterTwa() throws TimeoutException {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, NOTIFICATIONS, ContentSetting.ALLOW));
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
                                mOrigin, mPackage, NOTIFICATIONS, ContentSetting.ALLOW));
        assertTrue(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));

        runOnUiThreadBlocking(() -> InstalledWebappPermissionManager.unregister(mOrigin));
        assertFalse(ShortcutHelper.doesOriginContainAnyInstalledTwa(mOrigin.toString()));
    }

    public static class GeolocationContentSettingsParams implements ParameterProvider {
        private static final List<ParameterSet> sGeolocationContentSettingsParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(ContentSettingsType.GEOLOCATION)
                                .name("LegacyGeolocation"),
                        new ParameterSet()
                                .value(ContentSettingsType.GEOLOCATION_WITH_OPTIONS)
                                .name("GeolocationWithOptions"));

        @Override
        public List<ParameterSet> getParameters() {
            return sGeolocationContentSettingsParams;
        }
    }

    @UseMethodParameter(GeolocationContentSettingsParams.class)
    @Test
    @SmallTest
    public void allowGeolocation(@ContentSettingsType.EnumType int geolocation) {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, geolocation, ContentSetting.ALLOW));
        assertEquals(
                Integer.valueOf(ContentSetting.ALLOW),
                WebappRegistry.getInstance()
                        .getPermissionStore()
                        .getPermission(geolocation, mOrigin));
    }

    @UseMethodParameter(GeolocationContentSettingsParams.class)
    @Test
    @SmallTest
    public void blockGeolocation(@ContentSettingsType.EnumType int geolocation) {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin, mPackage, geolocation, ContentSetting.BLOCK));
        assertEquals(
                Integer.valueOf(ContentSetting.BLOCK),
                WebappRegistry.getInstance()
                        .getPermissionStore()
                        .getPermission(geolocation, mOrigin));
    }

    private String getNotificationPermission() throws TimeoutException {
        return mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab("Notification.permission");
    }
}
