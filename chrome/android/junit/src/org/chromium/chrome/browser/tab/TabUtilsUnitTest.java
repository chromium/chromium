// Copyright 2022 The Chromium Authors. All rights reserved.
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link TabUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {TabUtilsUnitTest.ShadowProfile.class,
                TabUtilsUnitTest.ShadowCriticalPersistedTabData.class})
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

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        ShadowCriticalPersistedTabData.setCriticalPersistedTabData(mCriticalPersistedTabData);
        ShadowProfile.setProfile(mProfile);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        doAnswer(invocation -> mRdsDefault)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingEnabled(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        when(mTab.isNativePage()).thenReturn(false);
        when(mTabNative.isNativePage()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTabNative.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
    }

    @After
    public void tearDown() {
        ShadowProfile.reset();
        ShadowCriticalPersistedTabData.reset();
    }

    @Test
    public void testSwitchUserAgent_NotForcedByUser() {
        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, false);
        verify(mNavigationController).setUseDesktopUserAgent(false, true);

        TabUtils.switchUserAgent(mTab, true, false);
        verify(mNavigationController).setUseDesktopUserAgent(true, true);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, false);
        verify(mNavigationController).setUseDesktopUserAgent(false, false);

        TabUtils.switchUserAgent(mTabNative, true, false);
        verify(mNavigationController).setUseDesktopUserAgent(true, false);

        // CriticalPersistedTabData#setUserAgent should not be used when it is not forcedByUser.
        verify(mCriticalPersistedTabData, never()).setUserAgent(anyInt());
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultMobile() {
        // Global setting is Mobile.
        mRdsDefault = false;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true);
        verify(mNavigationController).setUseDesktopUserAgent(false, true);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTab, true, true);
        verify(mNavigationController).setUseDesktopUserAgent(true, true);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DESKTOP);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true);
        verify(mNavigationController).setUseDesktopUserAgent(false, false);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTabNative, true, true);
        verify(mNavigationController).setUseDesktopUserAgent(true, false);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DESKTOP);
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultDesktop() {
        // Global setting is Desktop.
        mRdsDefault = true;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true);
        verify(mNavigationController).setUseDesktopUserAgent(false, true);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTab, true, true);
        verify(mNavigationController).setUseDesktopUserAgent(true, true);
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true);
        verify(mNavigationController).setUseDesktopUserAgent(false, false);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTabNative, true, true);
        verify(mNavigationController).setUseDesktopUserAgent(true, false);
        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DEFAULT);
    }
}
