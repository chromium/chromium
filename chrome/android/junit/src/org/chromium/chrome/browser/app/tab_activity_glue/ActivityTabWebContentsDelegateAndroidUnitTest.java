// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.app.Activity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroidUnitTest.ShadowWebContentsDarkModeController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.shadows.ShadowColorUtils;
import org.chromium.url.GURL;

/** Unit test for {@link ActivityTabWebContentsDelegateAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowColorUtils.class, ShadowWebContentsDarkModeController.class})
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
@DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
public class ActivityTabWebContentsDelegateAndroidUnitTest {
    @Implements(WebContentsDarkModeController.class)
    static class ShadowWebContentsDarkModeController {
        static boolean sGlobalSettingsEnabled;
        static GURL sBlockedUrl;

        @Implementation
        public static boolean isEnabledForUrl(BrowserContextHandle browserContextHandle, GURL url) {
            return sGlobalSettingsEnabled && !url.equals(sBlockedUrl);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Activity mActivity;
    @Mock Profile mProfile;
    @Mock WebContents mWebContents;
    @Mock Tab mTab;

    GURL mUrl1 = new GURL("https://url1.com");
    GURL mUrl2 = new GURL("https://url2.com");

    private ActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Before
    public void setup() {
        mTabWebContentsDelegateAndroid =
                new ActivityTabWebContentsDelegateAndroid(
                        mTab,
                        mActivity,
                        null,
                        false,
                        null,
                        null,
                        null,
                        mock(Supplier.class),
                        mock(Supplier.class),
                        mock(Supplier.class));

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mUrl1).when(mWebContents).getVisibleUrl();
    }

    @After
    public void tearDown() {
        ShadowWebContentsDarkModeController.sBlockedUrl = null;
    }

    @Test
    public void testIsNightMode() {
        ShadowColorUtils.sInNightMode = true;
        Assert.assertTrue(
                "#isNightModeEnabled is false.",
                mTabWebContentsDelegateAndroid.isNightModeEnabled());

        ShadowColorUtils.sInNightMode = false;
        Assert.assertFalse(
                "isNightModeEnabled is true.", mTabWebContentsDelegateAndroid.isNightModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testForceDarkWebContent_ForceEnabled() {
        assertForceDarkEnabledForWebContents(true);
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
        ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE
    })
    public void testForceDarkWebContent_ThemeSettingsFeatureDisabled() {
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_WebContentsNotReady() {
        doReturn(null).when(mTab).getWebContents();
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_LightTheme() {
        ShadowColorUtils.sInNightMode = false;
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_GlobalSettingDisabled() {
        ShadowColorUtils.sInNightMode = true;
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = false;
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_GlobalSettingEnabled() {
        ShadowColorUtils.sInNightMode = true;
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = true;
        assertForceDarkEnabledForWebContents(true);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_DisabledForUrl() {
        ShadowColorUtils.sInNightMode = true;
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = true;
        ShadowWebContentsDarkModeController.sBlockedUrl = mUrl1;
        assertForceDarkEnabledForWebContents(false);

        doReturn(mUrl2).when(mWebContents).getVisibleUrl();
        assertForceDarkEnabledForWebContents(true);
    }

    private void assertForceDarkEnabledForWebContents(boolean isEnabled) {
        Assert.assertEquals(
                "Value of #isForceDarkWebContentEnabled is different than test settings.",
                isEnabled,
                mTabWebContentsDelegateAndroid.isForceDarkWebContentEnabled());
    }
}
