// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.Application;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;

import androidx.browser.trusted.sharing.ShareData;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.IntentUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.Arrays;

/** JUnit test for WebappLauncherActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class WebappLauncherActivityTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
    }

    @After
    public void tearDown() {
        WebApkReparentingHandler.getInstance().clear();
    }

    /**
     * Test that WebappLauncherActivity modifies the passed-in intent so that
     * WebApkIntentDataProviderFactory#create() returns null if the intent does not refer to a valid
     * WebAPK.
     */
    @Test
    public void testTryCreateWebappInfoAltersIntentIfNotValidWebApk() {
        WebApkValidator.setDisableValidationForTesting(false);

        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        assertNotNull(WebApkIntentDataProviderFactory.create(intent));
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        assertNull(WebApkIntentDataProviderFactory.create(intent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for old-style WebAPKs. */
    @Test
    public void testOldStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        // WebAPK package name should be part of the intent URI to enable launching multiple
        // WebAPKs.
        assertEquals("webapp://webapk-" + WEBAPK_PACKAGE_NAME, launchIntent.getDataString());
        assertTrue((launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for new-style WebAPKs. */
    @Test
    public void testNewStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(
                SameTaskWebApkActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertEquals(0, launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    /** Test that WebappLauncherActivity unpacks the reparenting token into a trusted tab ID. */
    @Test
    public void testReparentingTokenUnpackedToTabId() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 10;
        Tab mockTab = mock(Tab.class);
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.getUserDataHost()).thenReturn(new UserDataHost());
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        Intent launchIntent = getNextStartedActivity();
        assertEquals(tabId, IntentHandler.getTabId(launchIntent));
        AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
    }

    /**
     * Test that an intent with only a URL causes an intent to {@link ChromeLauncherActivity} (ie, a
     * "launch in tab" action) rather than an intent to {@link SameTaskWebApkActivity} or {@link
     * WebappActivity}. In particular, that means that this intent did NOT make it past the security
     * checks that ensure that webapp launches must be triggered from trusted intents from Chrome.
     */
    @Test
    public void testUnauthenticatedNonWebApkIntentOpensInTabAndNotWebappMode() {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.putExtra(WebApkConstants.EXTRA_URL, START_URL);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());

        // And just in case, one with an empty-string WebAPK Package Name.
        intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.putExtra(WebApkConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, "");

        activity = Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithMacVerifiedIcon() {
        String icon = "sample_base64_icon";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_1");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon);
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertTrue(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, WebappConstants.EXTRA_IS_ICON_TRUSTED, false));
    }

    @Test
    public void testWebappLaunchWithLegacyMacDoesNotTrustIcon() {
        String icon = "sample_base64_icon";
        byte[] legacyMac = WebappAuthenticator.getMacForUrl(START_URL);
        assertNotNull(legacyMac);
        String macB64 = Base64.encodeToString(legacyMac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_2");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon);
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);
        intent.putExtra(WebappConstants.EXTRA_IS_ICON_TRUSTED, true);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertFalse(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, WebappConstants.EXTRA_IS_ICON_TRUSTED, false));
    }

    @Test
    public void testWebappLaunchWithModifiedIconOpensInTab() {
        String icon = "sample_base64_icon";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_3");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon + "_modified");
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithShiftedFieldBoundariesOpensInTab() {
        String icon = "PREFIX_SAMPLE_ICON";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_4");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL + "PREFIX_");
        intent.putExtra(WebappConstants.EXTRA_ICON, "SAMPLE_ICON");
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithMalformedMacOpensInTab() {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_5");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, "sample_base64_icon");
        intent.putExtra(WebappConstants.EXTRA_MAC, "!@#$invalid_base64%$#@!");

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    private void registerWebApk(String webApkPackage, String startUrl) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackage, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackage, startUrl);
    }

    private Intent getNextStartedActivity() {
        return shadowOf((Application) ApplicationProvider.getApplicationContext())
                .getNextStartedActivity();
    }

    @Test
    public void testWebApkShareIntent_StashesVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri uri = Uri.parse("content://org.chromium.webapk.test/file.jpg");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(uri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);
        intent.putExtra(Intent.EXTRA_SUBJECT, "title");
        intent.putExtra(Intent.EXTRA_TEXT, "text");

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals("title", verifiedData.title);
        assertEquals("text", verifiedData.text);
        assertEquals(1, verifiedData.uris.size());
        assertEquals(uri, verifiedData.uris.get(0));
    }

    @Test
    public void testWebApkLaunch_StripsSpoofedVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        Bundle spoofedBundle =
                new ShareData(
                                "spoofed",
                                "spoofed",
                                Arrays.asList(Uri.parse("content://victim/secret")))
                        .toBundle();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, spoofedBundle);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertFalse(launchIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA));
    }

    @Test
    public void testWebApkShareIntent_TextOnlyShare_StashesVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        intent.putExtra(Intent.EXTRA_SUBJECT, "title");
        intent.putExtra(Intent.EXTRA_TEXT, "text");

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals("title", verifiedData.title);
        assertEquals("text", verifiedData.text);
        assertTrue(verifiedData.uris == null || verifiedData.uris.isEmpty());
    }

    @Test
    public void testWebApkShareIntent_InvalidSchemeUri_FilteredOut() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri fileSchemeUri = Uri.parse("file:///sdcard/malicious.txt");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(fileSchemeUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertTrue(verifiedData.uris.isEmpty());
    }

    @Test
    public void testWebApkShareIntent_PartialFiltering_InvalidAndValidUri() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri fileSchemeUri = Uri.parse("file:///sdcard/malicious.txt");
        Uri validContentUri = Uri.parse("content://org.chromium.webapk.test/valid.jpg");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(fileSchemeUri);
        uris.add(validContentUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals(1, verifiedData.uris.size());
        assertEquals(validContentUri, verifiedData.uris.get(0));
    }
}
