// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.util.DisplayMetrics;
import android.util.Size;

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
import org.robolectric.annotation.Resetter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.tasks.tab_management.TabGridThumbnailView;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for {@link TabUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {TabUtilsUnitTest.ShadowProfile.class})
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

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

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

    private boolean mRdsDefault;
    private @ContentSettingValues int mRdsException;
    private boolean mIsGlobal;
    private boolean mUseDesktopUserAgent;
    private @TabUserAgent int mTabUserAgent;
    private @TabUserAgent int mTabNativeUserAgent;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
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
        doAnswer(invocation -> mIsGlobal)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingGlobal(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), any(), any());
        doAnswer(invocation -> mUseDesktopUserAgent)
                .when(mNavigationController)
                .getUseDesktopUserAgent();
        doAnswer(invocation -> mTabUserAgent).when(mTab).getUserAgent();
        doAnswer(invocation -> mTabNativeUserAgent).when(mTabNative).getUserAgent();
        doAnswer(invocation -> {
            mTabUserAgent = invocation.getArgument(0);
            return null;
        })
                .when(mTab)
                .setUserAgent(anyInt());
        doAnswer(invocation -> {
            mTabNativeUserAgent = invocation.getArgument(0);
            return null;
        })
                .when(mTabNative)
                .setUserAgent(anyInt());
    }

    @After
    public void tearDown() {
        ShadowProfile.reset();
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

        // Tab#setUserAgent should not be used when it is not forcedByUser.
        verify(mTab, never()).setUserAgent(anyInt());
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultMobile() {
        // Global setting is Mobile.
        mRdsDefault = false;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mTab).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTab, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mTab).setUserAgent(TabUserAgent.DESKTOP);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mTabNative).setUserAgent(TabUserAgent.DEFAULT);

        TabUtils.switchUserAgent(mTabNative, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mTabNative).setUserAgent(TabUserAgent.DESKTOP);
    }

    @Test
    public void testSwitchUserAgent_ForcedByUser_DefaultDesktop() {
        // Global setting is Desktop.
        mRdsDefault = true;

        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mTab).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTab, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mTab).setUserAgent(TabUserAgent.DEFAULT);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);
        verify(mTabNative).setUserAgent(TabUserAgent.MOBILE);

        TabUtils.switchUserAgent(mTabNative, true, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);
        verify(mTabNative).setUserAgent(TabUserAgent.DEFAULT);
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
        verify(mTab).setUserAgent(TabUserAgent.DEFAULT);

        mTabUserAgent = TabUserAgent.UNSET;
        mUseDesktopUserAgent = true;
        Assert.assertEquals("TabUserAgent is not set up correctly for upgrade path.",
                TabUserAgent.DESKTOP, TabUtils.getTabUserAgent(mTab));
        verify(mTab).setUserAgent(TabUserAgent.DESKTOP);
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

        verify(mTab, never()).setUserAgent(anyInt());
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

        verify(mTab, never()).setUserAgent(anyInt());
    }

    @Test
    public void testReadRequestDesktopSiteContentSettings() {
        GURL gurl = JUnitTestGURLs.EXAMPLE_URL;

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

    @Test
    public void testIsRequestDesktopSiteContentSettingsGlobal() {
        GURL gurl = JUnitTestGURLs.EXAMPLE_URL;

        // Content setting is global setting.
        mIsGlobal = true;
        Assert.assertTrue("The result should be true when there is no url",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, null));
        Assert.assertTrue("Content setting is global setting.",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, gurl));

        // Content setting is NOT global setting.
        mIsGlobal = false;
        Assert.assertTrue("The result should be true when there is no url",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, null));
        Assert.assertFalse("Content setting is domain setting.",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, gurl));
    }

    @Test
    public void testUpdateThumbnailMatrix_notOnAutomotiveDevice_thumbnailImageHasOriginalDensity() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        int mockImageSize = 100;
        int mockTargetSize = 50;

        TabGridThumbnailView thumbnailView = Mockito.mock(TabGridThumbnailView.class);
        Bitmap bitmap = Bitmap.createBitmap(mockImageSize, mockImageSize, Bitmap.Config.ARGB_8888);
        bitmap.setDensity(DisplayMetrics.DENSITY_DEFAULT);
        TabUtils.setBitmapAndUpdateImageMatrix(
                thumbnailView, bitmap, new Size(mockTargetSize, mockTargetSize));

        assertNotEquals("The bitmap image density should not be zero.", 0, bitmap.getDensity());
        assertEquals("The bitmap image's density should not be scaled up on non-automotive"
                        + " devices.",
                DisplayMetrics.DENSITY_DEFAULT, bitmap.getDensity());
    }

    @Test
    public void testUpdateThumbnailMatrix_onAutomotiveDevice_thumbnailImageHasScaledUpDensity() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        int mockImageSize = 100;
        int mockTargetSize = 50;

        TabGridThumbnailView thumbnailView = Mockito.mock(TabGridThumbnailView.class);
        Bitmap bitmap = Bitmap.createBitmap(mockImageSize, mockImageSize, Bitmap.Config.ARGB_8888);
        bitmap.setDensity(DisplayMetrics.DENSITY_DEFAULT);
        TabUtils.setBitmapAndUpdateImageMatrix(
                thumbnailView, bitmap, new Size(mockTargetSize, mockTargetSize));

        assertNotEquals("The bitmap image density should not be zero.", 0, bitmap.getDensity());
        assertEquals("The bitmap image's density should be scaled up on automotive.",
                (int) (DisplayMetrics.DENSITY_DEFAULT
                        * DisplayUtil.getUiScalingFactorForAutomotive()),
                bitmap.getDensity());
    }
}
