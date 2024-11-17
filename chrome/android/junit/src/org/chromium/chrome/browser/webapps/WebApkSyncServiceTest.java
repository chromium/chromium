// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.XmlResourceParserImpl;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.w3c.dom.Document;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.sync.protocol.WebApkSpecifics;

import java.io.ByteArrayInputStream;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/** Tests the WebApkSyncService class */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {BackgroundShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class WebApkSyncServiceTest {
    private static final String START_URL = "https://example.com/start";
    private static final String MANIFEST_ID = "https://example.com/id";
    private static final String PACKAGE_NAME = "org.chromium.webapk";
    private static final String SCOPE = "https://example.com/";
    private static final String NAME = "My App";
    private static final String SHORT_NAME = "app";
    private static final long TOOLBAR_COLOR = Color.WHITE;
    private static final int PRIMARY_ICON_ID = 12;
    private static final String ICON_URL = "https://example.com/icon.png";
    private static final String ICON_MURMUR2_HASH = "5";
    private static final String ICON_URL2 = "https://example.com/icon2.png";

    @Mock private Resources mMockResources;

    @Rule public FakeTimeTestRule mFakeClockRule = new FakeTimeTestRule();

    private String mPrimaryIconXmlContents;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private XmlResourceParserImpl getMockXmlResourceParserImpl() {
        try {
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            factory.setNamespaceAware(true);
            factory.setIgnoringComments(true);
            factory.setIgnoringElementContentWhitespace(true);
            DocumentBuilder documentBuilder = factory.newDocumentBuilder();
            Document document =
                    documentBuilder.parse(
                            new ByteArrayInputStream(mPrimaryIconXmlContents.getBytes()));

            return new XmlResourceParserImpl(document, "file", PACKAGE_NAME, PACKAGE_NAME, null);
        } catch (Exception e) {
            return null;
        }
    }

    public WebappDataStorage registerWebappAndGetStorage(String packageName) throws Exception {
        String webappId = WebappIntentUtils.getIdForWebApkPackage(packageName);
        CallbackHelper helper = new CallbackHelper();
        WebappRegistry.getInstance()
                .register(
                        webappId,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                helper.notifyCalled();
                            }
                        });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        helper.waitForOnly();

        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }

    @Test
    public void testGetWebApkSyncSpecifics() throws Exception {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(PACKAGE_NAME, START_URL)
                        .setWebApkManifestId(MANIFEST_ID)
                        .setName(NAME)
                        .setShortName(SHORT_NAME)
                        .setToolbarColor(TOOLBAR_COLOR)
                        .setScope(SCOPE)
                        .build();
        WebappInfo webApkInfo = WebappInfo.create(intentDataProvider);
        WebappDataStorage storage = registerWebappAndGetStorage(PACKAGE_NAME);
        storage.updateLastUsedTime();

        WebApkSpecifics webApkSpecifics = WebApkSyncService.getWebApkSpecifics(webApkInfo, storage);

        assertEquals(MANIFEST_ID, webApkSpecifics.getManifestId());
        assertEquals(START_URL, webApkSpecifics.getStartUrl());
        assertEquals(SCOPE, webApkSpecifics.getScope());
        assertTrue(webApkSpecifics.hasName());
        assertEquals(NAME, webApkSpecifics.getName());
        assertTrue(webApkSpecifics.hasThemeColor());
        assertEquals(TOOLBAR_COLOR, webApkSpecifics.getThemeColor());
        assertEquals(0, webApkSpecifics.getIconInfosCount());
        // From 1653000000000L May 19 2022 18:40:00 GMT-0400.
        assertEquals(13297473600000000L, webApkSpecifics.getLastUsedTimeWindowsEpochMicros());
    }

    @Test
    public void testGetSyncSpecificsWithEmptyValues() throws Exception {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(PACKAGE_NAME, START_URL)
                        .setWebApkManifestId(MANIFEST_ID)
                        .setShortName(SHORT_NAME)
                        .build();
        WebappInfo webApkInfo = WebappInfo.create(intentDataProvider);
        WebappDataStorage storage = registerWebappAndGetStorage(PACKAGE_NAME);

        WebApkSpecifics webApkSpecifics = WebApkSyncService.getWebApkSpecifics(webApkInfo, storage);

        assertEquals(MANIFEST_ID, webApkSpecifics.getManifestId());
        assertEquals(START_URL, webApkSpecifics.getStartUrl());
        assertEquals(SCOPE, webApkSpecifics.getScope());

        assertTrue(webApkSpecifics.hasName());
        assertEquals(SHORT_NAME, webApkSpecifics.getName());

        assertEquals(0, webApkSpecifics.getThemeColor());
        assertEquals(0, webApkSpecifics.getIconInfosCount());
    }

    @Test
    public void testGetIconInfo() throws Exception {
        mPrimaryIconXmlContents =
                String.format(
                        "<bitmap xmlns:android='http://schemas.android.com/apk/res/android'"
                                + "  android:src='@mipmap/app_icon_xxhdpi'"
                                + "  iconUrl='%s'"
                                + "  iconHash='%s'"
                                + "/>",
                        ICON_URL, ICON_MURMUR2_HASH);
        when(mMockResources.getXml(anyInt())).thenReturn(getMockXmlResourceParserImpl());

        WebappIcon testIcon =
                new WebappIcon(
                        PACKAGE_NAME,
                        PRIMARY_ICON_ID,
                        mMockResources,
                        WebappIcon.ICON_WITH_URL_AND_HASH_SHELL_VERSION);
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(PACKAGE_NAME, START_URL)
                        .setWebApkManifestId(MANIFEST_ID)
                        .setPrimaryIcon(testIcon)
                        .setShellApkVersion(WebappIcon.ICON_WITH_URL_AND_HASH_SHELL_VERSION)
                        .build();
        WebappInfo webApkInfo = WebappInfo.create(intentDataProvider);
        WebappDataStorage storage = registerWebappAndGetStorage(PACKAGE_NAME);

        WebApkSpecifics webApkSpecifics = WebApkSyncService.getWebApkSpecifics(webApkInfo, storage);

        assertEquals(1, webApkSpecifics.getIconInfosCount());
        assertEquals(ICON_URL, webApkSpecifics.getIconInfos(0).getUrl());
    }

    @Test
    public void testGetIconsFallback() throws Exception {
        WebappIcon testIcon = new WebappIcon();
        Map<String, String> iconUrlAndIconMurmur2HashMap = new HashMap<String, String>();
        iconUrlAndIconMurmur2HashMap.put(ICON_URL, ICON_MURMUR2_HASH);
        iconUrlAndIconMurmur2HashMap.put(ICON_URL2, ICON_MURMUR2_HASH);
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(PACKAGE_NAME, START_URL)
                        .setWebApkManifestId(MANIFEST_ID)
                        .setPrimaryIcon(testIcon)
                        .setIconUrlToMurmur2HashMap(iconUrlAndIconMurmur2HashMap)
                        .build();
        WebappInfo webApkInfo = WebappInfo.create(intentDataProvider);
        WebappDataStorage storage = registerWebappAndGetStorage(PACKAGE_NAME);

        WebApkSpecifics webApkSpecifics = WebApkSyncService.getWebApkSpecifics(webApkInfo, storage);

        assertEquals(2, webApkSpecifics.getIconInfosCount());
        assertEquals(ICON_URL, webApkSpecifics.getIconInfos(0).getUrl());
        assertEquals(ICON_URL2, webApkSpecifics.getIconInfos(1).getUrl());
    }
}
