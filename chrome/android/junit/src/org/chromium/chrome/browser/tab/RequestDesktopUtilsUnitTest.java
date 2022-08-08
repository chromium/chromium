// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link RequestDesktopUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public class RequestDesktopUtilsUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock
    private BrowserContextHandle mBrowserContextHandleMock;

    private @ContentSettingValues int mRdsDefaultValue;
    private final Map<String, Integer> mContentSettingMap = new HashMap<>();
    private final GURL mGoogleUrl = new GURL(JUnitTestGURLs.GOOGLE_URL);
    private final GURL mMapsUrl = new GURL(JUnitTestGURLs.MAPS_URL);
    private static final String GOOGLE_COM = "[*.]google.com/";

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        mJniMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);

        doAnswer(invocation -> mRdsDefaultValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getDefaultContentSetting(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));
        doAnswer(invocation -> {
            mContentSettingMap.put(invocation.getArgument(2), invocation.getArgument(4));
            return null;
        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingCustomScope(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE),
                        anyString(), anyString(), anyInt());
        doAnswer(invocation -> getDomainAndRegistry(invocation.getArgument(0)))
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(anyString(), anyBoolean());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGoogleUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mMapsUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGoogleUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mMapsUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow() {
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGoogleUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mMapsUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock() {
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGoogleUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mMapsUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    /**
     * Helper to get organization-identifying host from URLs. The real implementation calls
     * {@link UrlUtilities}. It's not useful to actually reimplement it, so just return a string in
     * a trivial way.
     * @param origin A URL.
     * @return The organization-identifying host from the given URL.
     */
    private String getDomainAndRegistry(String origin) {
        return origin.replaceAll(".*\\.(.+\\.[^.]+$)", "$1");
    }
}
