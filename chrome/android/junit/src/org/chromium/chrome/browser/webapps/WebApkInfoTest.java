// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests WebApkInfo.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkInfoTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String UNBOUND_WEBAPK_PACKAGE_NAME = "unbound.webapk";

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";
    private static final String SCOPE = "https://www.google.com/scope";
    private static final String NAME = "name";
    private static final String SHORT_NAME = "short_name";
    private static final String DISPLAY_MODE = "minimal-ui";
    private static final String ORIENTATION = "portrait";
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";
    private static final int SHELL_APK_VERSION = 3;
    private static final String MANIFEST_URL = "https://www.google.com/alphabet.json";
    private static final String ICON_URL = "https://www.google.com/scope/worm.png";
    private static final String ICON_MURMUR2_HASH = "5";
    private static final int SOURCE = ShortcutSource.NOTIFICATION;

    /** Fakes the Resources object, allowing lookup of String value. */
    private static class FakeResources extends Resources {
        private final Map<String, Integer> mStringIdMap;
        private final Map<Integer, String> mIdValueMap;

        // Do not warn about deprecated call to Resources(); the documentation says code is not
        // supposed to create its own Resources object, but we are using it to fake out the
        // Resources, and there is no other way to do that.
        @SuppressWarnings("deprecation")
        public FakeResources() {
            super(new AssetManager(), null, null);
            mStringIdMap = new HashMap<>();
            mIdValueMap = new HashMap<>();
        }

        @Override
        public int getIdentifier(String name, String defType, String defPackage) {
            String key = getKey(name, defType, defPackage);
            return mStringIdMap.containsKey(key) ? mStringIdMap.get(key) : 0;
        }

        @Override
        public String getString(int id) {
            if (!mIdValueMap.containsKey(id)) {
                throw new Resources.NotFoundException("id 0x" + Integer.toHexString(id));
            }

            return mIdValueMap.get(id);
        }

        @Override
        public int getColor(int id, Resources.Theme theme) {
            return Integer.parseInt(getString(id));
        }

        public void addStringForTesting(
                String name, String defType, String defPackage, int identifier, String value) {
            String key = getKey(name, defType, defPackage);
            mStringIdMap.put(key, identifier);
            mIdValueMap.put(identifier, value);
        }

        public void addColorForTesting(String name, String defPackage, int identifier, int value) {
            addStringForTesting(name, "color", defPackage, identifier, Integer.toString(value));
        }

        private String getKey(String name, String defType, String defPackage) {
            return defPackage + ":" + defType + "/" + name;
        }
    }

    /**
     * Returns simplest intent which builds valid WebApkInfo via {@link WebApkInfo#create()}.
     */
    private static Intent createMinimalWebApkIntent(String webApkPackage, String url) {
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackage);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        return intent;
    }

    @Test
    public void testSanity() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.SCOPE, SCOPE);
        bundle.putString(WebApkMetaDataKeys.NAME, NAME);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, SHORT_NAME);
        bundle.putString(WebApkMetaDataKeys.DISPLAY_MODE, DISPLAY_MODE);
        bundle.putString(WebApkMetaDataKeys.ORIENTATION, ORIENTATION);
        bundle.putString(WebApkMetaDataKeys.THEME_COLOR, THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, BACKGROUND_COLOR);
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, SHELL_APK_VERSION);
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, MANIFEST_URL);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL + " " + ICON_MURMUR2_HASH);

        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "action0");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "title0");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "text0");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name1\", \"name2\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS,
                "[[\"text/plain\"], [\"image/png\", \"image/jpeg\"]]");

        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, true);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);

        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME, info.id());
        Assert.assertEquals(START_URL, info.url());
        Assert.assertTrue(info.shouldForceNavigation());
        Assert.assertEquals(SCOPE, info.scopeUrl());
        Assert.assertEquals(NAME, info.name());
        Assert.assertEquals(SHORT_NAME, info.shortName());
        Assert.assertEquals(WebDisplayMode.MINIMAL_UI, info.displayMode());
        Assert.assertEquals(ScreenOrientationValues.PORTRAIT, info.orientation());
        Assert.assertTrue(info.hasValidToolbarColor());
        Assert.assertEquals(1L, info.toolbarColor());
        Assert.assertTrue(info.hasValidBackgroundColor());
        Assert.assertEquals(2L, info.backgroundColor());
        Assert.assertEquals(WEBAPK_PACKAGE_NAME, info.webApkPackageName());
        Assert.assertEquals(SHELL_APK_VERSION, info.shellApkVersion());
        Assert.assertEquals(MANIFEST_URL, info.manifestUrl());
        Assert.assertEquals(START_URL, info.manifestStartUrl());
        Assert.assertEquals(WebApkDistributor.BROWSER, info.distributor());

        Assert.assertEquals(1, info.iconUrlToMurmur2HashMap().size());
        Assert.assertTrue(info.iconUrlToMurmur2HashMap().containsKey(ICON_URL));
        Assert.assertEquals(ICON_MURMUR2_HASH, info.iconUrlToMurmur2HashMap().get(ICON_URL));

        Assert.assertEquals(SOURCE, info.source());
        Assert.assertEquals(
                (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M), info.isSplashProvidedByWebApk());

        Assert.assertEquals(null, info.icon().bitmap());
        Assert.assertEquals(null, info.badgeIcon().bitmap());
        Assert.assertEquals(null, info.splashIcon().bitmap());

        WebApkInfo.ShareTarget shareTarget = info.shareTarget();
        Assert.assertNotNull(shareTarget);
        Assert.assertEquals("action0", shareTarget.getAction());
        Assert.assertTrue(shareTarget.isShareMethodPost());
        Assert.assertTrue(shareTarget.isShareEncTypeMultipart());
        Assert.assertEquals("title0", shareTarget.getParamTitle());
        Assert.assertEquals("text0", shareTarget.getParamText());
        Assert.assertEquals(new String[] {"name1", "name2"}, shareTarget.getFileNames());
        Assert.assertEquals(new String[][] {{"text/plain"}, {"image/png", "image/jpeg"}},
                shareTarget.getFileAccepts());
    }

    /**
     * Test that {@link WebApkInfo#create()} populates {@link WebApkInfo#url()} with the start URL
     * from the intent not the start URL in the WebAPK's meta data. When a WebAPK is launched via a
     * deep link from a URL within the WebAPK's scope, the WebAPK should open at the URL it was deep
     * linked from not the WebAPK's start URL.
     */
    @Test
    public void testUseStartUrlOverride() {
        String intentStartUrl = "https://www.google.com/master_override";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, intentStartUrl);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(intentStartUrl, info.url());

        // {@link WebApkInfo#manifestStartUrl()} should contain the start URL from the Android
        // Manifest.
        Assert.assertEquals(START_URL, info.manifestStartUrl());
    }

    /**
     * Test that if the scope is empty that the scope is computed from the "start URL specified from
     * the Web Manifest" not the "URL the WebAPK initially navigated to". Deep links can open a
     * WebAPK at an arbitrary URL.
     */
    @Test
    public void testDefaultScopeFromManifestStartUrl() {
        String manifestStartUrl = START_URL;
        String intentStartUrl = "https://www.google.com/a/b/c";

        String scopeFromManifestStartUrl = ShortcutHelper.getScopeFromUrl(manifestStartUrl);
        String scopeFromIntentStartUrl = ShortcutHelper.getScopeFromUrl(intentStartUrl);
        Assert.assertNotEquals(scopeFromManifestStartUrl, scopeFromIntentStartUrl);

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, "");
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, intentStartUrl);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(scopeFromManifestStartUrl, info.scopeUrl());
    }

    /**
     * Test that {@link WebApkInfo#create} can read multiple icon URLs and multiple icon murmur2
     * hashes from the WebAPK's meta data.
     */
    @Test
    public void testGetIconUrlAndMurmur2HashFromMetaData() {
        String iconUrl1 = "/icon1.png";
        String murmur2Hash1 = "1";
        String iconUrl2 = "/icon2.png";
        String murmur2Hash2 = "2";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                iconUrl1 + " " + murmur2Hash1 + " " + iconUrl2 + " " + murmur2Hash2);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);
        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(2, iconUrlToMurmur2HashMap.size());
        Assert.assertEquals(murmur2Hash1, iconUrlToMurmur2HashMap.get(iconUrl1));
        Assert.assertEquals(murmur2Hash2, iconUrlToMurmur2HashMap.get(iconUrl2));
    }

    /**
     * WebApkIconHasher generates hashes with values [0, 2^64-1]. 2^64-1 is greater than
     * {@link Long#MAX_VALUE}. Test that {@link WebApkInfo#create()} can read a hash with value
     * 2^64 - 1.
     */
    @Test
    public void testGetIconMurmur2HashFromMetaData() {
        String hash = "18446744073709551615"; // 2^64 - 1

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES, "randomUrl " + hash);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);
        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(1, iconUrlToMurmur2HashMap.size());
        Assert.assertTrue(iconUrlToMurmur2HashMap.containsValue(hash));
    }

    /**
     * Prior to SHELL_APK_VERSION 2, WebAPKs did not specify
     * {@link ShortcutHelper#EXTRA_FORCE_NAVIGATION} in the intent. Test that
     * {@link WebApkInfo#shouldForceNavigation()} defaults to true when the intent extra is not
     * specified.
     */
    @Test
    public void testForceNavigationNotSpecified() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertTrue(info.shouldForceNavigation());
    }

    /**
     * Test that {@link WebApkInfo#source()} returns {@link ShortcutSource#UNKNOWN} if the source
     * in the launch intent > {@link ShortcutSource#COUNT}. This can occur if the user is using a
     * new WebAPK and an old version of Chrome.
     */
    @Test
    public void testOutOfBoundsSource() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.COUNT + 1);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(ShortcutSource.UNKNOWN, info.source());
    }

    /**
     * Test that {@link WebApkInfo#name()} and {@link WebApkInfo#shortName()} return the name and
     * short name from the meta data before they are moved to strings in resources.
     */
    @Test
    public void testNameAndShortNameFromMetadataWhenStringResourcesDoNotExist() {
        String name = "WebAPK name";
        String shortName = "WebAPK short name";
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.NAME, name);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, shortName);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(name, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    /**
     * Test that {@link WebApkInfo#name()} and {@link WebApkInfo#shortName()} return the string
     * values from the WebAPK resources if exist.
     */
    @Test
    public void testNameAndShortNameFromWebApkStrings() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        String name = "WebAPK name";
        String shortName = "WebAPK short name";
        FakeResources res = new FakeResources();
        res.addStringForTesting(WebApkIntentDataProvider.RESOURCE_NAME,
                WebApkIntentDataProvider.RESOURCE_STRING_TYPE, WEBAPK_PACKAGE_NAME, 1, name);
        res.addStringForTesting(WebApkIntentDataProvider.RESOURCE_SHORT_NAME,
                WebApkIntentDataProvider.RESOURCE_STRING_TYPE, WEBAPK_PACKAGE_NAME, 2, shortName);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(name, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    /**
     * Test that ShortcutSource#EXTERNAL_INTENT is rewritten to
     * ShortcutSource#EXTERNAL_INTENT_FROM_CHROME if the WebAPK is launched from a
     * browser with the same package name (e.g. web page link on Chrome Stable
     * launches WebAPK whose host browser is Chrome Stable).
     */
    @Test
    public void testOverrideExternalIntentSourceIfLaunchedFromChrome() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.EXTERNAL_INTENT);
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, RuntimeEnvironment.application.getPackageName());

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(ShortcutSource.EXTERNAL_INTENT_FROM_CHROME, info.source());
    }

    /**
     * Test that ShortcutSource#EXTERNAL_INTENT is not rewritten when the WebAPK is launched
     * from a non-browser app.
     */
    @Test
    public void testOverrideExternalIntentSourceIfLaunchedFromNonChromeApp() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.EXTERNAL_INTENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.google.android.talk");

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(ShortcutSource.EXTERNAL_INTENT, info.source());
    }

    /**
     * Test that ShortcutSource#SHARE_TARGET is rewritten to
     * ShortcutSource#WEBAPK_SHARE_TARGET_FILE if the WebAPK is launched as a result of user sharing
     * a binary file.
     */
    @Test
    public void testOverrideShareTargetSourceIfLaunchedFromFileSharing() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "something");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-3"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.WEBAPK_SHARE_TARGET);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(ShortcutSource.WEBAPK_SHARE_TARGET_FILE, info.source());
    }

    /**
     * Test when a distributor is not specified, the default distributor value for a WebAPK
     * installed by Chrome is |WebApkDistributor.BROWSER|, while for an Unbound WebAPK is
     * |WebApkDistributor.Other|.
     */
    @Test
    public void testWebApkDistributorDefaultValue() {
        // Test Case: Bound WebAPK
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);
        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(WebApkDistributor.BROWSER, info.distributor());

        // Test Case: Unbound WebAPK
        WebApkTestHelper.registerWebApkWithMetaData(
                UNBOUND_WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);
        intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, UNBOUND_WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        info = WebApkInfo.create(intent);
        Assert.assertEquals(WebApkDistributor.OTHER, info.distributor());
    }

    /**
     * Test that {@link WebApkInfo#shareTarget()} returns a non-null but empty object if the WebAPK
     * does not handle share intents.
     */
    @Test
    public void testGetShareTargetNotNullEvenIfDoesNotHandleShareIntents() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);
        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertNotNull(info.shareTarget());
        Assert.assertEquals("", info.shareTarget().getAction());
    }

    /**
     * Tests that {@link WebApkInfo.ShareTarget#getFileNames()} returns an empty list when the
     * {@link shareParamNames} <meta-data> tag is not a JSON array.
     */
    @Test
    public void testPostShareTargetInvalidParamNames() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);

        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "not an array");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});
        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkInfo.ShareTarget shareTarget = info.shareTarget();
        Assert.assertNotNull(shareTarget);
        Assert.assertEquals(0, shareTarget.getFileNames().length);
    }

    /**
     * Tests building {@link WebApkInfo.ShareData} when {@link Intent.EXTRA_STREAM} has a Uri value.
     */
    @Test
    public void testShareDataUriString() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        Uri sharedFileUri = Uri.parse("mock-uri-1");
        intent.putExtra(Intent.EXTRA_STREAM, sharedFileUri);

        WebApkInfo info = WebApkInfo.create(intent);
        WebApkInfo.ShareData shareData = info.shareData();
        Assert.assertNotNull(shareData);
        Assert.assertNotNull(shareData.files);
        Assert.assertThat(shareData.files, Matchers.contains(sharedFileUri));
    }

    /**
     * Tests building {@link WebApkInfo.ShareData} when {@link Intent.EXTRA_STREAM} has an ArrayList
     * value.
     */
    @Test
    public void testShareDataUriList() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        Uri sharedFileUri = Uri.parse("mock-uri-1");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(sharedFileUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);
        WebApkInfo.ShareData shareData = info.shareData();
        Assert.assertNotNull(shareData);
        Assert.assertNotNull(shareData.files);
        Assert.assertThat(shareData.files, Matchers.contains(sharedFileUri));
    }

    /**
     * Test that {@link WebApkInfo#backgroundColorFallbackToDefault()} uses
     * {@link SplashLayout#getDefaultBackgroundColor()} as the default background color if there is
     * no default background color in the WebAPK's resources.
     */
    @Test
    public void testBackgroundColorFallbackToDefaultNoCustomDefault() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING + "L");
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(SplashLayout.getDefaultBackgroundColor(RuntimeEnvironment.application),
                info.backgroundColorFallbackToDefault());
    }

    /**
     * Test that {@link WebApkInfo#backgroundColorFallbackToDefault()} uses the default
     * background color from the WebAPK's resources if present.
     */
    @Test
    public void testBackgroundColorFallbackToDefaultWebApkHasCustomDefault() {
        final int defaultBackgroundColorResourceId = 1;
        final int defaultBackgroundColorInWebApk = 42;

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING + "L");
        bundle.putInt(
                WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID, defaultBackgroundColorResourceId);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);

        FakeResources res = new FakeResources();
        res.addColorForTesting("mockResource", WEBAPK_PACKAGE_NAME,
                defaultBackgroundColorResourceId, defaultBackgroundColorInWebApk);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(
                defaultBackgroundColorInWebApk, info.backgroundColorFallbackToDefault());
    }
}
