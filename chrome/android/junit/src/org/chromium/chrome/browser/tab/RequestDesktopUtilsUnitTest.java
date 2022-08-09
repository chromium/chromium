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

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowSysUtils;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
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
import java.util.Map.Entry;

/**
 * Unit tests for {@link RequestDesktopUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class, ShadowSysUtils.class})
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
    private SharedPreferencesManager mSharedPreferencesManager;

    private final Map<String, Integer> mContentSettingMap = new HashMap<>();
    private final GURL mGoogleUrl = new GURL(JUnitTestGURLs.GOOGLE_URL);
    private final GURL mMapsUrl = new GURL(JUnitTestGURLs.MAPS_URL);

    private final TestValues mTestValues = new TestValues();

    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        private static boolean sLowEndDevice;

        public static void setLowEndDevice(boolean lowEndDevice) {
            sLowEndDevice = lowEndDevice;
        }

        @Implementation
        public static boolean isLowEndDevice() {
            return sLowEndDevice;
        }
    }

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
            mRdsDefaultValue = invocation.getArgument(2) ? ContentSettingValues.ALLOW
                                                         : ContentSettingValues.BLOCK;
            return null;
        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingEnabled(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), anyBoolean());

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

        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.disableKeyCheckerForTesting();
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
        ShadowSysUtils.setLowEndDevice(false);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING);
        mSharedPreferencesManager.removeKey(
                SingleCategorySettings.USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
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

    @Test
    public void testShouldDefaultEnableGlobalSetting_DisableOnLowEndDevice() {
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_LOW_END_DEVICES, "false");
        enableFeatureRequestDesktopSiteDefaults(params);
        ShadowSysUtils.setLowEndDevice(true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils
                        .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on low memory devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_CustomScreenSizeThreshold() {
        Map<String, String> params = new HashMap<>();
        params.put(
                RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                "10.0");
        enableFeatureRequestDesktopSiteDefaults(params);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(11.0);
        Assert.assertTrue("Desktop site global setting should be default-enabled on 10\"+ devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_UserPreviouslyUpdatedSetting() {
        enableFeatureRequestDesktopSiteDefaults(null);
        // This SharedPreference key will ideally be updated when the user explicitly requests for
        // an update to the desktop site global setting.
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettings.USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils
                        .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled if it has been previously updated by the user.",
                shouldDefaultEnable);
    }

    @Test
    public void testMaybeDefaultEnableGlobalSetting() {
        enableFeatureRequestDesktopSiteDefaults(null);
        boolean didDefaultEnable = RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                Mockito.mock(Profile.class));
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on big screen devices.",
                didDefaultEnable);
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                ContentSettingValues.ALLOW, mRdsDefaultValue);
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be true.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING,
                                false));

        // Check whether the desktop site global setting should be default-enabled after the first
        // attempt.
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils
                        .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled more than once.",
                shouldDefaultEnable);
    }

    private void enableFeatureRequestDesktopSiteDefaults(Map<String, String> params) {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, true);
        if (params != null) {
            for (Entry<String, String> param : params.entrySet()) {
                mTestValues.addFieldTrialParamOverride(
                        ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, param.getKey(),
                        param.getValue());
            }
        }
        FeatureList.setTestValues(mTestValues);
    }
}
