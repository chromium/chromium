// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.app.Activity;
import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroidUnitTest.ShadowColorUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.ui.util.ColorUtils;

/** Unit test for ActivityTabWebContentsDelegateAndroid. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowColorUtils.class)
public class ActivityTabWebContentsDelegateAndroidUnitTest {
    private static final int TAB_ID = 123;

    @Implements(ColorUtils.class)
    static class ShadowColorUtils {
        static boolean sInNightMode;
        @Implementation
        public static boolean inNightMode(Context context) {
            return sInNightMode;
        }
    }

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    Activity mActivity;
    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJni;
    @Mock
    Profile mProfile;

    private ActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Before
    public void setup() {
        mTabWebContentsDelegateAndroid =
                new ActivityTabWebContentsDelegateAndroid(new MockTab(TAB_ID, false), mActivity,
                        null, false, null, null, null, Mockito.mock(Supplier.class),
                        Mockito.mock(Supplier.class), Mockito.mock(Supplier.class));
        Profile.setLastUsedProfileForTesting(mProfile);
    }

    @Test
    public void testIsNightMode() {
        ShadowColorUtils.sInNightMode = true;
        Assert.assertTrue("#isNightModeEnabled is false.",
                mTabWebContentsDelegateAndroid.isNightModeEnabled());

        ShadowColorUtils.sInNightMode = false;
        Assert.assertFalse(
                "isNightModeEnabled is true.", mTabWebContentsDelegateAndroid.isNightModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testForceDarkWebContent_ForceEnabled() {
        assertForceDarkWebContentEnabled(true);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
            ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE})
    public void
    testForceDarkWebContent_ThemeSettingsFeatureDisabled() {
        assertForceDarkWebContentEnabled(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testForceDarkWebContent_GlobalSettingsEnabled_LightTheme() {
        setGlobalSettingsEnabled(true);
        ShadowColorUtils.sInNightMode = false;
        assertForceDarkWebContentEnabled(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testForceDarkWebContent_GlobalSettingsEnabled_DarkTheme() {
        setGlobalSettingsEnabled(true);
        ShadowColorUtils.sInNightMode = true;
        assertForceDarkWebContentEnabled(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testForceDarkWebContent_GlobalSettingsDisabled() {
        setGlobalSettingsEnabled(false);
        ShadowColorUtils.sInNightMode = true;
        assertForceDarkWebContentEnabled(false);
    }

    private void assertForceDarkWebContentEnabled(boolean isEnabled) {
        Assert.assertEquals(
                "Value of #isForceDarkWebContentEnabled is different than test settings.",
                isEnabled, mTabWebContentsDelegateAndroid.isForceDarkWebContentEnabled());
    }

    private void setGlobalSettingsEnabled(boolean isEnabled) {
        mMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJni);
        Mockito.doReturn(isEnabled)
                .when(mWebsitePreferenceBridgeJni)
                .isContentSettingEnabled(ArgumentMatchers.notNull(), ArgumentMatchers.anyInt());
    }
}
