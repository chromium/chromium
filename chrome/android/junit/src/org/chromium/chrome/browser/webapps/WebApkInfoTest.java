// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.provider.Browser;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

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

        public void addStringForTesting(
                String name, String defType, String defPackage, int identifier, String value) {
            String key = getKey(name, defType, defPackage);
            mStringIdMap.put(key, identifier);
            mIdValueMap.put(identifier, value);
        }

        private String getKey(String name, String defType, String defPackage) {
            return defPackage + ":" + defType + "/" + name;
        }
    }

    @Before
    public void setUp() {
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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, true);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
        intent.putExtra(WebApkConstants.EXTRA_USE_TRANSPARENT_SPLASH, true);

        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME, info.id());
        Assert.assertEquals(START_URL, info.uri().toString());
        Assert.assertTrue(info.shouldForceNavigation());
        Assert.assertEquals(SCOPE, info.scopeUri().toString());
        Assert.assertEquals(NAME, info.name());
        Assert.assertEquals(SHORT_NAME, info.shortName());
        Assert.assertEquals(WebDisplayMode.MINIMAL_UI, info.displayMode());
        Assert.assertEquals(ScreenOrientationValues.PORTRAIT, info.orientation());
        Assert.assertTrue(info.hasValidThemeColor());
        Assert.assertEquals(1L, info.themeColor());
        Assert.assertTrue(info.hasValidBackgroundColor());
        Assert.assertEquals(2L, info.backgroundColor());
        Assert.assertEquals(WEBAPK_PACKAGE_NAME, info.apkPackageName());
        Assert.assertEquals(SHELL_APK_VERSION, info.shellApkVersion());
        Assert.assertEquals(MANIFEST_URL, info.manifestUrl());
        Assert.assertEquals(START_URL, info.manifestStartUrl());
        Assert.assertEquals(WebApkInfo.WebApkDistributor.BROWSER, info.distributor());

        Assert.assertEquals(1, info.iconUrlToMurmur2HashMap().size());
        Assert.assertTrue(info.iconUrlToMurmur2HashMap().containsKey(ICON_URL));
        Assert.assertEquals(ICON_MURMUR2_HASH, info.iconUrlToMurmur2HashMap().get(ICON_URL));

        Assert.assertEquals(SOURCE, info.source());
        Assert.assertTrue(info.useTransparentSplash());

        Assert.assertEquals(null, info.icon());
        Assert.assertEquals(null, info.badgeIcon());
        Assert.assertEquals(null, info.splashIcon());
    }

    /**
     * Test that {@link WebApkInfo#create()} populates {@link WebApkInfo#uri()} with the start URL
     * from the intent not the start URL in the WebAPK's meta data. When a WebAPK is launched via a
     * deep link from a URL within the WebAPK's scope, the WebAPK should open at the URL it was deep
     * linked from not the WebAPK's start URL.
     */
    @Test
    public void testUseStartUrlOverride() {
        String intentStartUrl = "https://www.google.com/master_override";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, intentStartUrl);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(intentStartUrl, info.uri().toString());

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, intentStartUrl);

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(scopeFromManifestStartUrl, info.scopeUri().toString());
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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        String name = "WebAPK name";
        String shortName = "WebAPK short name";
        FakeResources res = new FakeResources();
        res.addStringForTesting(WebApkInfo.RESOURCE_NAME, WebApkInfo.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME, 1, name);
        res.addStringForTesting(WebApkInfo.RESOURCE_SHORT_NAME, WebApkInfo.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME, 2, shortName);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
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
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.EXTERNAL_INTENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.google.android.talk");

        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(ShortcutSource.EXTERNAL_INTENT, info.source());
    }

    /**
     * Test when a distributor is not specified, the default distributor value for a WebAPK
     * installed by Chrome is |WebApkInfo.WebApkDistributor.BROWSER|, while for an Unbound WebAPK is
     * |WebApkInfo.WebApkDistributor.Other|.
     */
    @Test
    public void testWebApkDistributorDefaultValue() {
        // Test Case: Bound WebAPK
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        WebApkInfo info = WebApkInfo.create(intent);
        Assert.assertEquals(WebApkInfo.WebApkDistributor.BROWSER, info.distributor());

        // Test Case: Unbound WebAPK
        WebApkTestHelper.registerWebApkWithMetaData(UNBOUND_WEBAPK_PACKAGE_NAME, bundle);
        intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, UNBOUND_WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        info = WebApkInfo.create(intent);
        Assert.assertEquals(WebApkInfo.WebApkDistributor.OTHER, info.distributor());
    }
}
