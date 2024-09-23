// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;

import androidx.browser.trusted.sharing.ShareData;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.XmlResourceParserImpl;
import org.robolectric.annotation.Config;
import org.robolectric.res.ResourceTable;
import org.robolectric.util.ReflectionHelpers;
import org.w3c.dom.Document;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/** Tests {@link WebappInfo} with WebAPKs. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkInfoTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String UNBOUND_WEBAPK_PACKAGE_NAME = "unbound.webapk";

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";
    private static final String SCOPE = "https://www.google.com/scope";
    private static final String MANIFEST_ID = "https://www.google.com/id";
    private static final String APP_KEY = "https://www.google.com/key";
    private static final String NAME = "name";
    private static final String SHORT_NAME = "short_name";
    private static final String DISPLAY_MODE = "minimal-ui";
    private static final String ORIENTATION = "portrait";
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";
    private static final String DARK_THEME_COLOR = "3L";
    private static final String DARK_BACKGROUND_COLOR = "4L";
    private static final int SHELL_APK_VERSION = 3;
    private static final String MANIFEST_URL = "https://www.google.com/alphabet.json";
    private static final String ICON_URL = "https://www.google.com/scope/worm.png";
    private static final String ICON_MURMUR2_HASH = "5";
    private static final int PRIMARY_ICON_ID = 12;
    private static final int PRIMARY_MASKABLE_ICON_ID = 14;
    private static final int SOURCE = ShortcutSource.NOTIFICATION;

    /** Fakes the Resources object, allowing lookup of String value. */
    private static class FakeResources extends Resources {
        private static AssetManager sAssetManager = createAssetManager();
        private final Map<String, Integer> mStringIdMap;
        private final Map<Integer, String> mIdValueMap;
        private String mShortcutsXmlContents;
        private String mPrimaryIconXmlContents;

        private class MockXmlResourceParserImpl extends XmlResourceParserImpl {
            String mPackageName;

            public MockXmlResourceParserImpl(
                    Document document,
                    String fileName,
                    String packageName,
                    String applicationPackageName,
                    ResourceTable resourceTable) {
                super(document, fileName, packageName, applicationPackageName, resourceTable);
                mPackageName = packageName;
            }

            @Override
            public int getAttributeResourceValue(
                    String namespace, String attribute, int defaultValue) {
                // Remove the trailing '@'.
                String attributeValue = getAttributeValue(namespace, attribute).substring(1);
                if (mStringIdMap.containsKey(attributeValue)) {
                    return mStringIdMap.get(attributeValue);
                }
                return defaultValue;
            }
        }

        private static AssetManager createAssetManager() {
            try {
                return AssetManager.class.getConstructor().newInstance();
            } catch (Exception e) {
                return null;
            }
        }

        // Do not warn about deprecated call to Resources(); the documentation says code is not
        // supposed to create its own Resources object, but we are using it to fake out the
        // Resources, and there is no other way to do that.
        @SuppressWarnings("deprecation")
        public FakeResources() {
            super(sAssetManager, null, null);
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

        @Override
        public XmlResourceParser getXml(int id) {
            String xmlContent;
            if (id == PRIMARY_ICON_ID || id == PRIMARY_MASKABLE_ICON_ID) {
                xmlContent = mPrimaryIconXmlContents;
            } else {
                xmlContent = mShortcutsXmlContents;
            }
            try {
                DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
                factory.setNamespaceAware(true);
                factory.setIgnoringComments(true);
                factory.setIgnoringElementContentWhitespace(true);
                DocumentBuilder documentBuilder = factory.newDocumentBuilder();
                Document document =
                        documentBuilder.parse(new ByteArrayInputStream(xmlContent.getBytes()));

                return new MockXmlResourceParserImpl(
                        document, "file", WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME, null);
            } catch (Exception e) {
                Assert.fail("Failed to create XmlResourceParser");
                return null;
            }
        }

        void setShortcutsXmlContent(String content) {
            mShortcutsXmlContents = content;
        }

        void setPrimaryIconXmlContents(String content) {
            mPrimaryIconXmlContents = content;
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

    @Test
    public void testSanity() {
        // Test guidelines:
        // - Stubbing out native calls in this test likely means that there is a bug.
        // - For every WebappInfo boolean there should be a test which tests both values.

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.SCOPE, SCOPE);
        bundle.putString(WebApkMetaDataKeys.NAME, NAME);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, SHORT_NAME);
        bundle.putBoolean(WebApkMetaDataKeys.HAS_CUSTOM_NAME, true);
        bundle.putString(WebApkMetaDataKeys.DISPLAY_MODE, DISPLAY_MODE);
        bundle.putString(WebApkMetaDataKeys.ORIENTATION, ORIENTATION);
        bundle.putString(WebApkMetaDataKeys.THEME_COLOR, THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, BACKGROUND_COLOR);
        bundle.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, DARK_THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.DARK_BACKGROUND_COLOR, DARK_BACKGROUND_COLOR);
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, SHELL_APK_VERSION);
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, MANIFEST_URL);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_ID, MANIFEST_ID);
        bundle.putString(WebApkMetaDataKeys.APP_KEY, APP_KEY);
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL + " " + ICON_MURMUR2_HASH);
        bundle.putInt(WebApkMetaDataKeys.ICON_ID, PRIMARY_ICON_ID);
        bundle.putInt(WebApkMetaDataKeys.MASKABLE_ICON_ID, PRIMARY_MASKABLE_ICON_ID);

        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "action0");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "title0");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "text0");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name1\", \"name2\"]");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS,
                "[[\"text/plain\"], [\"image/png\", \"image/jpeg\"]]");

        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebappConstants.EXTRA_FORCE_NAVIGATION, true);
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);

        WebappInfo info = createWebApkInfo(intent);

        Assert.assertEquals(WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME, info.id());
        Assert.assertEquals(START_URL, info.url());
        Assert.assertTrue(info.shouldForceNavigation());
        Assert.assertEquals(SCOPE, info.scopeUrl());
        Assert.assertEquals(NAME, info.name());
        Assert.assertEquals(SHORT_NAME, info.shortName());
        Assert.assertEquals(true, info.hasCustomName());
        Assert.assertEquals(MANIFEST_ID, info.manifestId());
        Assert.assertEquals(APP_KEY, info.appKey());
        Assert.assertEquals(DisplayMode.MINIMAL_UI, info.displayMode());
        Assert.assertEquals(ScreenOrientationLockType.PORTRAIT, info.orientation());
        Assert.assertTrue(info.hasValidToolbarColor());
        Assert.assertEquals(1L, info.toolbarColor());
        Assert.assertTrue(info.hasValidBackgroundColor());
        Assert.assertEquals(2L, info.backgroundColor());
        Assert.assertTrue(info.hasValidDarkToolbarColor());
        Assert.assertEquals(3L, info.darkToolbarColor());
        Assert.assertTrue(info.hasValidDarkBackgroundColor());
        Assert.assertEquals(4L, info.darkBackgroundColor());
        Assert.assertEquals(WEBAPK_PACKAGE_NAME, info.webApkPackageName());
        Assert.assertEquals(SHELL_APK_VERSION, info.shellApkVersion());
        Assert.assertEquals(MANIFEST_URL, info.manifestUrl());
        Assert.assertEquals(START_URL, info.manifestStartUrl());
        Assert.assertEquals(WebApkDistributor.BROWSER, info.distributor());

        Assert.assertEquals(1, info.iconUrlToMurmur2HashMap().size());
        Assert.assertTrue(info.iconUrlToMurmur2HashMap().containsKey(ICON_URL));
        Assert.assertEquals(ICON_MURMUR2_HASH, info.iconUrlToMurmur2HashMap().get(ICON_URL));

        Assert.assertEquals(SOURCE, info.source());
        Assert.assertTrue(info.isSplashProvidedByWebApk());

        Assert.assertEquals(PRIMARY_MASKABLE_ICON_ID, info.icon().resourceIdForTesting());
        Assert.assertEquals(true, info.isIconAdaptive());
        Assert.assertEquals(null, info.splashIcon().bitmap());

        WebApkShareTarget shareTarget = info.shareTarget();
        Assert.assertNotNull(shareTarget);
        Assert.assertEquals("action0", shareTarget.getAction());
        Assert.assertTrue(shareTarget.isShareMethodPost());
        Assert.assertTrue(shareTarget.isShareEncTypeMultipart());
        Assert.assertEquals("title0", shareTarget.getParamTitle());
        Assert.assertEquals("text0", shareTarget.getParamText());
        Assert.assertEquals(new String[] {"name1", "name2"}, shareTarget.getFileNames());
        Assert.assertEquals(
                new String[][] {{"text/plain"}, {"image/png", "image/jpeg"}},
                shareTarget.getFileAccepts());
    }

    /**
     * Test that {@link createWebApkInfo()} ignores the maskable icon on pre-Android-O Android OSes.
     */
    @Test
    public void testOsVersionDoesNotSupportAdaptive() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.N);

        Bundle bundle = new Bundle();
        bundle.putInt(WebApkMetaDataKeys.ICON_ID, PRIMARY_ICON_ID);
        bundle.putInt(WebApkMetaDataKeys.MASKABLE_ICON_ID, PRIMARY_MASKABLE_ICON_ID);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(PRIMARY_ICON_ID, info.icon().resourceIdForTesting());
        Assert.assertEquals(false, info.isIconAdaptive());
    }

    /**
     * Test that {@link createWebApkInfo()} selects {@link WebApkMetaDataKeys.ICON_ID} if no
     * maskable icon is provided and that the icon is tagged as non-maskable.
     */
    @Test
    public void testNoMaskableIcon() {
        Bundle bundle = new Bundle();
        bundle.putInt(WebApkMetaDataKeys.ICON_ID, PRIMARY_ICON_ID);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(PRIMARY_ICON_ID, info.icon().resourceIdForTesting());
        Assert.assertEquals(false, info.isIconAdaptive());
    }

    /**
     * Test that {@link createWebApkInfo()} populates {@link WebappInfo#url()} with the start URL
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
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, intentStartUrl);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(intentStartUrl, info.url());

        // {@link WebappInfo#manifestStartUrl()} should contain the start URL from the Android
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
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, intentStartUrl);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(scopeFromManifestStartUrl, info.scopeUrl());
    }

    /**
     * Test that {@link createWebApkInfo} can read multiple icon URLs and multiple icon murmur2
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
        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                iconUrl1 + " " + murmur2Hash1 + " " + iconUrl2 + " " + murmur2Hash2);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(2, iconUrlToMurmur2HashMap.size());
        Assert.assertEquals(murmur2Hash1, iconUrlToMurmur2HashMap.get(iconUrl1));
        Assert.assertEquals(murmur2Hash2, iconUrlToMurmur2HashMap.get(iconUrl2));
    }

    /**
     * WebApkIconHasher generates hashes with values [0, 2^64-1]. 2^64-1 is greater than {@link
     * Long#MAX_VALUE}. Test that {@link createWebApkInfo()} can read a hash with value 2^64 - 1.
     */
    @Test
    public void testGetIconMurmur2HashFromMetaData() {
        String hash = "18446744073709551615"; // 2^64 - 1

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES, "randomUrl " + hash);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Map<String, String> iconUrlToMurmur2HashMap = info.iconUrlToMurmur2HashMap();
        Assert.assertEquals(1, iconUrlToMurmur2HashMap.size());
        Assert.assertTrue(iconUrlToMurmur2HashMap.containsValue(hash));
    }

    /**
     * Prior to SHELL_APK_VERSION 2, WebAPKs did not specify {@link
     * WebappConstants#EXTRA_FORCE_NAVIGATION} in the intent. Test that {@link
     * WebappInfo#shouldForceNavigation()} defaults to true when the intent extra is not specified.
     */
    @Test
    public void testForceNavigationNotSpecified() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertTrue(info.shouldForceNavigation());
    }

    /**
     * Test that {@link WebappInfo#source()} returns {@link ShortcutSource#UNKNOWN} if the source in
     * the launch intent > {@link ShortcutSource#COUNT}. This can occur if the user is using a new
     * WebAPK and an old version of Chrome.
     */
    @Test
    public void testOutOfBoundsSource() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.COUNT + 1);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(ShortcutSource.UNKNOWN, info.source());
    }

    /**
     * Test that {@link WebappInfo#name()} and {@link WebappInfo#shortName()} return the name and
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
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(name, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    /**
     * Test that {@link WebappInfo#name()} and {@link WebappInfo#shortName()} return the string
     * values from the WebAPK resources if exist.
     */
    @Test
    public void testNameAndShortNameFromWebApkStrings() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        String name = "WebAPK name";
        String shortName = "WebAPK short name";
        FakeResources res = new FakeResources();
        res.addStringForTesting(
                WebApkIntentDataProviderFactory.RESOURCE_NAME,
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                1,
                name);
        res.addStringForTesting(
                WebApkIntentDataProviderFactory.RESOURCE_SHORT_NAME,
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                2,
                shortName);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(name, info.name());
        Assert.assertEquals(shortName, info.shortName());
    }

    /**
     * Test that ShortcutSource#EXTERNAL_INTENT is rewritten to
     * ShortcutSource#EXTERNAL_INTENT_FROM_CHROME if the WebAPK is launched from a browser with the
     * same package name (e.g. web page link on Chrome Stable launches WebAPK whose host browser is
     * Chrome Stable).
     */
    @Test
    public void testOverrideExternalIntentSourceIfLaunchedFromChrome() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.EXTERNAL_INTENT);
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, RuntimeEnvironment.application.getPackageName());

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(ShortcutSource.EXTERNAL_INTENT_FROM_CHROME, info.source());
    }

    /**
     * Test that ShortcutSource#EXTERNAL_INTENT is not rewritten when the WebAPK is launched from a
     * non-browser app.
     */
    @Test
    public void testOverrideExternalIntentSourceIfLaunchedFromNonChromeApp() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.EXTERNAL_INTENT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.google.android.talk");

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(ShortcutSource.EXTERNAL_INTENT, info.source());
    }

    /**
     * Test that ShortcutSource#SHARE_TARGET is rewritten to ShortcutSource#WEBAPK_SHARE_TARGET_FILE
     * if the WebAPK is launched as a result of user sharing a binary file.
     */
    @Test
    public void testOverrideShareTargetSourceIfLaunchedFromFileSharing() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "something");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-3"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);
        intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.WEBAPK_SHARE_TARGET);

        WebappInfo info = createWebApkInfo(intent);
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
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(WebApkDistributor.BROWSER, info.distributor());

        // Test Case: Unbound WebAPK
        WebApkTestHelper.registerWebApkWithMetaData(
                UNBOUND_WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
        intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, UNBOUND_WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        info = createWebApkInfo(intent);
        Assert.assertEquals(WebApkDistributor.OTHER, info.distributor());
    }

    /**
     * Test that {@link WebappInfo#shareTarget()} returns a null object if the WebAPK does not
     * handle share intents.
     */
    @Test
    public void testGetShareTargetNullIfDoesNotHandleShareIntents() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);

        Assert.assertNull(info.shareTarget());
    }

    /**
     * Tests that {@link WebApkShareTarget#getFileNames()} returns an empty list when the {@link
     * shareParamNames} <meta-data> tag is not a JSON array.
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
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);

        WebApkShareTarget shareTarget = info.shareTarget();
        Assert.assertNotNull(shareTarget);
        Assert.assertEquals(0, shareTarget.getFileNames().length);
    }

    /** Tests building {@link ShareData} when {@link Intent.EXTRA_STREAM} has a Uri value. */
    @Test
    public void testShareDataUriString() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        Uri sharedFileUri = Uri.parse("mock-uri-1");
        intent.putExtra(Intent.EXTRA_STREAM, sharedFileUri);

        WebappInfo info = createWebApkInfo(intent);
        ShareData shareData = info.shareData();
        Assert.assertNotNull(shareData);
        Assert.assertNotNull(shareData.uris);
        assertThat(shareData.uris, Matchers.contains(sharedFileUri));
    }

    /** Tests building {@link ShareData} when {@link Intent.EXTRA_STREAM} has an ArrayList value. */
    @Test
    public void testShareDataUriList() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, new Bundle[] {shareActivityBundle});

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        Uri sharedFileUri = Uri.parse("mock-uri-1");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(sharedFileUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebappInfo info = createWebApkInfo(intent);
        ShareData shareData = info.shareData();
        Assert.assertNotNull(shareData);
        Assert.assertNotNull(shareData.uris);
        assertThat(shareData.uris, Matchers.contains(sharedFileUri));
    }

    /**
     * Test that {@link WebappInfo#backgroundColorFallbackToDefault()} uses {@link
     * SplashLayout#getDefaultBackgroundColor()} as the default background color if there is no
     * default background color in the WebAPK's resources.
     */
    @Test
    public void testBackgroundColorFallbackToDefaultNoCustomDefault() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, ColorUtils.INVALID_COLOR + "L");
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(
                SplashLayout.getDefaultBackgroundColor(RuntimeEnvironment.application),
                info.backgroundColorFallbackToDefault());
    }

    /**
     * Test that {@link WebappInfo#backgroundColorFallbackToDefault()} uses the default background
     * color from the WebAPK's resources if present.
     */
    @Test
    public void testBackgroundColorFallbackToDefaultWebApkHasCustomDefault() {
        final int defaultBackgroundColorResourceId = 1;
        final int defaultBackgroundColorInWebApk = 42;

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, ColorUtils.INVALID_COLOR + "L");
        bundle.putInt(
                WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID, defaultBackgroundColorResourceId);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle, null);

        FakeResources res = new FakeResources();
        res.addColorForTesting(
                "mockResource",
                WEBAPK_PACKAGE_NAME,
                defaultBackgroundColorResourceId,
                defaultBackgroundColorInWebApk);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);

        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(
                defaultBackgroundColorInWebApk, info.backgroundColorFallbackToDefault());
    }

    /** Test that shortcut items are properly parsed. */
    @Test
    public void testShortcutItemsFromWebApkStrings() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        FakeResources res = new FakeResources();
        res.addStringForTesting(
                WebApkIntentDataProviderFactory.RESOURCE_SHORTCUTS,
                WebApkIntentDataProviderFactory.RESOURCE_XML_TYPE,
                WEBAPK_PACKAGE_NAME,
                1,
                null);
        res.addStringForTesting(
                "shortcut_1_short_name",
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                2,
                "short name1");
        res.addStringForTesting(
                "shortcut_1_name",
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                3,
                "name1");
        res.addStringForTesting(
                "shortcut_2_short_name",
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                4,
                "short name2");
        res.addStringForTesting(
                "shortcut_2_name",
                WebApkIntentDataProviderFactory.RESOURCE_STRING_TYPE,
                WEBAPK_PACKAGE_NAME,
                5,
                "name2");
        res.addStringForTesting("shortcut_1_icon", "drawable", WEBAPK_PACKAGE_NAME, 6, null);
        res.addStringForTesting("shortcut_2_icon", "drawable", WEBAPK_PACKAGE_NAME, 7, null);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        // No shortcuts case.
        res.setShortcutsXmlContent(
                "<shortcuts xmlns:android='http://schemas.android.com/apk/res/android'/>");
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(info.shortcutItems().size(), 0);

        // One shortcut case.
        String oneShortcut =
                "<shortcuts xmlns:android='http://schemas.android.com/apk/res/android'>"
                        + "  <shortcut"
                        + "    android:shortcutId='shortcut_1'"
                        + "    android:icon='@drawable/shortcut_1_icon'"
                        + "    iconUrl='https://example.com/icon1.png'"
                        + "    iconHash='1234'"
                        + "    android:shortcutShortLabel='@string/shortcut_1_short_name'"
                        + "    android:shortcutLongLabel='@string/shortcut_1_name'>"
                        + "      <intent android:data='https://example.com/launch1' />"
                        + "  </shortcut>"
                        + "</shortcuts>";

        res.setShortcutsXmlContent(oneShortcut);
        info = createWebApkInfo(intent);
        Assert.assertEquals(info.shortcutItems().size(), 1);
        WebApkExtras.ShortcutItem item = info.shortcutItems().get(0);
        Assert.assertEquals(item.name, "name1");
        Assert.assertEquals(item.shortName, "short name1");
        Assert.assertEquals(item.launchUrl, "https://example.com/launch1");
        Assert.assertEquals(item.iconUrl, "https://example.com/icon1.png");
        Assert.assertEquals(item.iconHash, "1234");
        Assert.assertNotNull(item.icon);
        Assert.assertEquals(item.icon.resourceIdForTesting(), 6);

        // Multiple shortcuts case.
        String twoShortcuts =
                "<shortcuts xmlns:android='http://schemas.android.com/apk/res/android'>"
                        + "  <shortcut"
                        + "    android:shortcutId='shortcut_1'"
                        + "    android:icon='@drawable/shortcut_1_icon'"
                        + "    iconUrl='https://example.con/icon1.png'"
                        + "    iconHash='1234'"
                        + "    android:shortcutShortLabel='@string/shortcut_1_short_name'"
                        + "    android:shortcutLongLabel='@string/shortcut_1_name'>"
                        + "      <intent android:data='https://example.com/launch1' />"
                        + "  </shortcut>"
                        + "  <shortcut"
                        + "    android:shortcutId='shortcut_2'"
                        + "    android:icon='@drawable/shortcut_2_icon'"
                        + "    iconUrl='https://example.com/icon2.png'"
                        + "    iconHash='2345'"
                        + "    android:shortcutShortLabel='@string/shortcut_2_short_name'"
                        + "    android:shortcutLongLabel='@string/shortcut_2_name'>"
                        + "      <intent android:data='https://example.com/launch2' />"
                        + "  </shortcut>"
                        + "</shortcuts>";

        res.setShortcutsXmlContent(twoShortcuts);
        info = createWebApkInfo(intent);
        Assert.assertEquals(info.shortcutItems().size(), 2);
        item = info.shortcutItems().get(1);
        Assert.assertEquals(item.name, "name2");
        Assert.assertEquals(item.shortName, "short name2");
        Assert.assertEquals(item.launchUrl, "https://example.com/launch2");
        Assert.assertEquals(item.iconUrl, "https://example.com/icon2.png");
        Assert.assertEquals(item.iconHash, "2345");
        Assert.assertNotNull(item.icon);
        Assert.assertEquals(item.icon.resourceIdForTesting(), 7);
    }

    /** Test appKey is set to the WEB_MANIFEST_URL if APP_KEY is not specified. */
    @Test
    public void testAppKeyFallbackManifestUrl() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, MANIFEST_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(MANIFEST_URL, info.appKey());
    }

    private WebappInfo createWebApkInfo(Intent intent) {
        return WebappInfo.create(WebApkIntentDataProviderFactory.create(intent));
    }

    /** Test get icon url and hash from xml for SHELL_APK_VERSION 169+. */
    @Test
    public void testIconWithUrlAndHash() {
        Bundle bundle = new Bundle();
        bundle.putInt(WebApkMetaDataKeys.ICON_ID, PRIMARY_ICON_ID);
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        // Set min shell version that contains the url and hash field.
        bundle.putInt(
                WebApkMetaDataKeys.SHELL_APK_VERSION,
                WebappIcon.ICON_WITH_URL_AND_HASH_SHELL_VERSION);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        FakeResources res = new FakeResources();
        String iconXml =
                String.format(
                        "<bitmap xmlns:android='http://schemas.android.com/apk/res/android'"
                                + "  android:src='@mipmap/app_icon_xxhdpi'"
                                + "  iconUrl='%s'"
                                + "  iconHash='%s'"
                                + "/>",
                        ICON_URL, ICON_MURMUR2_HASH);

        res.setPrimaryIconXmlContents(iconXml);
        WebApkTestHelper.setResource(WEBAPK_PACKAGE_NAME, res);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebappInfo info = createWebApkInfo(intent);
        Assert.assertEquals(PRIMARY_ICON_ID, info.icon().resourceIdForTesting());
        Assert.assertEquals(ICON_URL, info.icon().iconUrl());
        Assert.assertEquals(ICON_MURMUR2_HASH, info.icon().iconHash());
        Assert.assertEquals(false, info.isIconAdaptive());
    }

    /** Test get manifestId and fallbacks */
    @Test
    public void testManifestIdAndFallback() {
        {
            Bundle bundle = new Bundle();
            bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
            bundle.putString(WebApkMetaDataKeys.WEB_MANIFEST_ID, MANIFEST_ID);
            WebApkTestHelper.registerWebApkWithMetaData(
                    WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
            Intent intent =
                    WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
            WebappInfo info = createWebApkInfo(intent);
            Assert.assertEquals(START_URL, info.url());
            Assert.assertEquals(START_URL, info.manifestStartUrl());
            Assert.assertEquals(MANIFEST_ID, info.manifestId());
            Assert.assertEquals(MANIFEST_ID, info.manifestIdWithFallback());
        }

        {
            Bundle bundle = new Bundle();
            bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
            WebApkTestHelper.registerWebApkWithMetaData(
                    WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
            Intent intent =
                    WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
            WebappInfo info = createWebApkInfo(intent);
            Assert.assertEquals(START_URL, info.url());
            Assert.assertEquals(START_URL, info.manifestStartUrl());
            Assert.assertNull(info.manifestId());
            Assert.assertEquals(START_URL, info.manifestIdWithFallback());
        }
    }
}
