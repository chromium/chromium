// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

/**
 * Unit tests for {@link TabUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {TabUtilsUnitTest.ShadowProfile.class,
                TabUtilsUnitTest.ShadowCriticalPersistedTabData.class, ShadowGURL.class})
public class TabUtilsUnitTest {
    /**
     * A fake {@link Profile} used to reduce dependency.
     */
    @Implements(Profile.class)
    static class ShadowProfile {
        private static Profile sProfile;

        static void setProfile(Profile profile) {
            sProfile = profile;
        }

        @Resetter
        static void reset() {
            sProfile = null;
        }

        @Implementation
        public static Profile fromWebContents(WebContents webContents) {
            return sProfile;
        }
    }

    /**
     * A fake {@link CriticalPersistedTabData} used to reduce dependency.
     */
    @Implements(CriticalPersistedTabData.class)
    static class ShadowCriticalPersistedTabData {
        private static CriticalPersistedTabData sCriticalPersistedTabData;

        static void setCriticalPersistedTabData(CriticalPersistedTabData criticalPersistedTabData) {
            sCriticalPersistedTabData = criticalPersistedTabData;
        }

        @Resetter
        static void reset() {
            sCriticalPersistedTabData = null;
        }

        @Implementation
        public static CriticalPersistedTabData from(Tab tab) {
            return sCriticalPersistedTabData;
        }
    }

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTabNative;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private Profile mProfile;
    @Mock
    private CriticalPersistedTabData mCriticalPersistedTabData;

    private boolean mRdsDefault;
    private @ContentSettingValues int mRdsException;
    private boolean mUseDesktopUserAgent;
    private @TabUserAgent int mTabUserAgent;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        ShadowCriticalPersistedTabData.setCriticalPersistedTabData(mCriticalPersistedTabData);
        ShadowProfile.setProfile(mProfile);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        when(mTab.isNativePage()).thenReturn(false);
        when(mTabNative.isNativePage()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTabNative.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);

        doAnswer(invocation -> mRdsDefault)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingEnabled(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));
        doAnswer(invocation -> mRdsException)
                .when(mWebsitePreferenceBridgeJniMock)
                .getContentSetting(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), any(), any());
        doAnswer(invocation -> mUseDesktopUserAgent)
                .when(mNavigationController)
                .getUseDesktopUserAgent();
        doAnswer(invocation -> mTabUserAgent).when(mCriticalPersistedTabData).getUserAgent();
        doAnswer(invocation -> {
            mTabUserAgent = invocation.getArgument(0);
            return null;
        })
                .when(mCriticalPersistedTabData)
                .setUserAgent(anyInt());
    }

    @After
    public void tearDown() {
        ShadowProfile.reset();
        ShadowCriticalPersistedTabData.reset();
    }

    @Test
    public void testSwitchUserAgent_NotForcedByUser() {
        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);

        TabUtils.switchUserAgent(mTab, true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);

        TabUtils.switchUserAgent(mTabNative, true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);

        // CriticalPersistedTabData#setUserAgent should not be used when it is not forcedByUser.
        verify(mCriticalPersistedTabData, never()).setUserAgent(anyInt());
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultMobile() {
        // Global setting is Mobile.
        mRdsDefault = false;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTab, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DESKTOP);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTabNative, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DESKTOP);
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultDesktop() {
        // Global setting is Desktop.
        mRdsDefault = true;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTab, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTabNative, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DEFAULT);
    }

    @Test
    public void testIsUsingDesktopUserAgent() {
        Assert.assertFalse("The result should be false when there is no webContents.",
                TabUtils.isUsingDesktopUserAgent(null));
        mUseDesktopUserAgent = false;
        Assert.assertFalse(
                "Should get RDS from WebContents.", TabUtils.isUsingDesktopUserAgent(mWebContents));
        mUseDesktopUserAgent = true;
        Assert.assertTrue(
                "Should get RDS from WebContents.", TabUtils.isUsingDesktopUserAgent(mWebContents));
    }

    @Test
    public void testGetTabUserAgent_UpgradePath() {
        mTabUserAgent = TabUserAgent.UNSET;
        mUseDesktopUserAgent = false;
        Assert.assertEquals("TabUserAgent is not set up correctly for upgrade path.",
                TabUserAgent.DEFAULT, TabUtils.getTabUserAgent(mTab));
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);

        mTabUserAgent = TabUserAgent.UNSET;
        mUseDesktopUserAgent = true;
        Assert.assertEquals("TabUserAgent is not set up correctly for upgrade path.",
                TabUserAgent.DESKTOP, TabUtils.getTabUserAgent(mTab));
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DESKTOP);
    }

    @Test
    public void testGetTabUserAgent_Mobile() {
        mTabUserAgent = TabUserAgent.MOBILE;
        mUseDesktopUserAgent = false;
        Assert.assertEquals("Read unexpected TabUserAgent value.", TabUserAgent.MOBILE,
                TabUtils.getTabUserAgent(mTab));

        mUseDesktopUserAgent = true;
        Assert.assertEquals("Read unexpected TabUserAgent value.", TabUserAgent.MOBILE,
                TabUtils.getTabUserAgent(mTab));

        verify(mCriticalPersistedTabData, never()).setUserAgent(anyInt());
    }

    @Test
    public void testGetTabUserAgent_Desktop() {
        mTabUserAgent = TabUserAgent.DESKTOP;
        mUseDesktopUserAgent = false;
        Assert.assertEquals("Read unexpected TabUserAgent value.", TabUserAgent.DESKTOP,
                TabUtils.getTabUserAgent(mTab));

        mUseDesktopUserAgent = true;
        Assert.assertEquals("Read unexpected TabUserAgent value.", TabUserAgent.DESKTOP,
                TabUtils.getTabUserAgent(mTab));

        verify(mCriticalPersistedTabData, never()).setUserAgent(anyInt());
    }

    @Test
    public void testReadRequestDesktopSiteContentSettings_DesktopSiteExceptionDisabled() {
        enableDesktopSiteException(false);
        GURL gurl = new GURL(JUnitTestGURLs.EXAMPLE_URL);

        // Global setting is Mobile.
        mRdsDefault = false;
        Assert.assertFalse("The result should match RDS global setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));

        // Global setting is Desktop.
        mRdsDefault = true;
        Assert.assertTrue("The result should match RDS global setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));
    }

    @Test
    public void testReadRequestDesktopSiteContentSettings_DesktopSiteExceptionEnabled() {
        enableDesktopSiteException(true);
        GURL gurl = new GURL(JUnitTestGURLs.EXAMPLE_URL);

        // Site level setting is Mobile.
        mRdsException = ContentSettingValues.BLOCK;
        Assert.assertFalse("The result should be false when there is no url",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, null));
        Assert.assertFalse("The result should match RDS site level setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));

        // Site level setting is Desktop.
        mRdsException = ContentSettingValues.ALLOW;
        Assert.assertFalse("The result should be false when there is no url",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, null));
        Assert.assertTrue("The result should match RDS site level setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));
    }

    private void enableDesktopSiteException(boolean enable) {
        TestValues features = new TestValues();
        features.addFeatureFlagOverride(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, enable);
        FeatureList.setTestValues(features);
    }
}
