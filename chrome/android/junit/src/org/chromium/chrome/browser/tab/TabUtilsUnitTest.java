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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Insets;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.util.Size;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {TabUtilsUnitTest.ShadowProfile.class})
public class TabUtilsUnitTest {
    /** A fake {@link Profile} used to reduce dependency. */
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

    @Implements(WindowMetrics.class)
    public static class ShadowWindowMetrics {
        @Implementation
        public static WindowInsets getWindowInsets() {
            return sTestSysBarInsets;
        }
    }

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final int TEST_SCREEN_WIDTH = 1000;
    private static final int TEST_SCREEN_HEIGHT = 1000;
    private static final int TEST_NAVIGATION_BAR_HEIGHT = 30;

    private static final int TEST_STATUS_BAR_HEIGHT = 30;
    private static WindowInsets sTestSysBarInsets;

    @Mock WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private Tab mTab;
    @Mock private Tab mTabNative;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private Profile mProfile;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private Resources mResources;
    @Mock private Configuration mConfiguration;
    @Mock private DisplayMetrics mDisplayMetrics;

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
        doAnswer(
                        invocation -> {
                            mTabUserAgent = invocation.getArgument(0);
                            return null;
                        })
                .when(mTab)
                .setUserAgent(anyInt());
        doAnswer(
                        invocation -> {
                            mTabNativeUserAgent = invocation.getArgument(0);
                            return null;
                        })
                .when(mTabNative)
                .setUserAgent(anyInt());
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            sTestSysBarInsets =
                    new WindowInsets.Builder()
                            .setInsets(
                                    WindowInsets.Type.systemBars(),
                                    Insets.of(
                                            0,
                                            TEST_STATUS_BAR_HEIGHT,
                                            0,
                                            TEST_NAVIGATION_BAR_HEIGHT))
                            .build();
        }
    }

    @After
    public void tearDown() {
        ShadowProfile.reset();
    }

    @Test
    public void testSwitchUserAgent() {
        // Test non-native tab.
        TabUtils.switchUserAgent(mTab, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, true, UseDesktopUserAgentCaller.OTHER);

        TabUtils.switchUserAgent(mTab, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, true, UseDesktopUserAgentCaller.OTHER);

        // Test native tab.
        TabUtils.switchUserAgent(mTabNative, false, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(false, false, UseDesktopUserAgentCaller.OTHER);

        TabUtils.switchUserAgent(mTabNative, true, UseDesktopUserAgentCaller.OTHER);
        verify(mNavigationController)
                .setUseDesktopUserAgent(true, false, UseDesktopUserAgentCaller.OTHER);
    }

    @Test
    public void testIsUsingDesktopUserAgent() {
        Assert.assertFalse(
                "The result should be false when there is no webContents.",
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
        Assert.assertEquals(
                "TabUserAgent is not set up correctly for upgrade path.",
                TabUserAgent.DEFAULT,
                TabUtils.getTabUserAgent(mTab));
        verify(mTab).setUserAgent(TabUserAgent.DEFAULT);

        mTabUserAgent = TabUserAgent.UNSET;
        mUseDesktopUserAgent = true;
        Assert.assertEquals(
                "TabUserAgent is not set up correctly for upgrade path.",
                TabUserAgent.DESKTOP,
                TabUtils.getTabUserAgent(mTab));
        verify(mTab).setUserAgent(TabUserAgent.DESKTOP);
    }

    @Test
    public void testGetTabUserAgent_Mobile() {
        mTabUserAgent = TabUserAgent.MOBILE;
        mUseDesktopUserAgent = false;
        Assert.assertEquals(
                "Read unexpected TabUserAgent value.",
                TabUserAgent.MOBILE,
                TabUtils.getTabUserAgent(mTab));

        mUseDesktopUserAgent = true;
        Assert.assertEquals(
                "Read unexpected TabUserAgent value.",
                TabUserAgent.MOBILE,
                TabUtils.getTabUserAgent(mTab));

        verify(mTab, never()).setUserAgent(anyInt());
    }

    @Test
    public void testGetTabUserAgent_Desktop() {
        mTabUserAgent = TabUserAgent.DESKTOP;
        mUseDesktopUserAgent = false;
        Assert.assertEquals(
                "Read unexpected TabUserAgent value.",
                TabUserAgent.DESKTOP,
                TabUtils.getTabUserAgent(mTab));

        mUseDesktopUserAgent = true;
        Assert.assertEquals(
                "Read unexpected TabUserAgent value.",
                TabUserAgent.DESKTOP,
                TabUtils.getTabUserAgent(mTab));

        verify(mTab, never()).setUserAgent(anyInt());
    }

    @Test
    public void testReadRequestDesktopSiteContentSettings() {
        GURL gurl = JUnitTestGURLs.EXAMPLE_URL;

        // Site level setting is Mobile.
        mRdsException = ContentSettingValues.BLOCK;
        Assert.assertFalse(
                "The result should be false when there is no url",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, null));
        Assert.assertFalse(
                "The result should match RDS site level setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));

        // Site level setting is Desktop.
        mRdsException = ContentSettingValues.ALLOW;
        Assert.assertFalse(
                "The result should be false when there is no url",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, null));
        Assert.assertTrue(
                "The result should match RDS site level setting.",
                TabUtils.readRequestDesktopSiteContentSettings(mProfile, gurl));
    }

    @Test
    public void testIsRequestDesktopSiteContentSettingsGlobal() {
        GURL gurl = JUnitTestGURLs.EXAMPLE_URL;

        // Content setting is global setting.
        mIsGlobal = true;
        Assert.assertTrue(
                "The result should be true when there is no url",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, null));
        Assert.assertTrue(
                "Content setting is global setting.",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, gurl));

        // Content setting is NOT global setting.
        mIsGlobal = false;
        Assert.assertTrue(
                "The result should be true when there is no url",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, null));
        Assert.assertFalse(
                "Content setting is domain setting.",
                TabUtils.isRequestDesktopSiteContentSettingsGlobal(mProfile, gurl));
    }

    @Test
    @Config(
            sdk = Build.VERSION_CODES.R,
            shadows = {ShadowWindowMetrics.class},
            qualifiers = "w" + TEST_SCREEN_WIDTH + "dp-h" + TEST_SCREEN_HEIGHT + "dp-land")
    public void testGetTabThumbnailAspectRatioWithHorizontalAutomotiveToolbar() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.forceHorizontalAutomotiveToolbarForTesting(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int horizontalAutomotiveToolbarHeightDp =
                                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(
                                            activity);
                            callAndVerifyGetTabThumbnailAspectRatio(
                                    activity, horizontalAutomotiveToolbarHeightDp, 0);
                        });
    }

    @Test
    @Config(
            sdk = Build.VERSION_CODES.R,
            shadows = {ShadowWindowMetrics.class},
            qualifiers = "w" + TEST_SCREEN_WIDTH + "dp-h" + TEST_SCREEN_HEIGHT + "dp-land")
    public void testGetTabThumbnailAspectRatioWithVerticalAutomotiveToolbar() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Activity spyActivity = spy(activity);
                            int verticalAutomotiveToolbarWidthDp =
                                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(
                                            spyActivity);
                            callAndVerifyGetTabThumbnailAspectRatio(
                                    spyActivity, 0, verticalAutomotiveToolbarWidthDp);
                        });
    }

    @Test
    public void testUpdateThumbnailMatrix_notOnAutomotiveDevice_thumbnailImageHasOriginalDensity() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        int mockImageSize = 100;
        int mockTargetSize = 50;

        TabThumbnailView thumbnailView = Mockito.mock(TabThumbnailView.class);
        Bitmap bitmap = Bitmap.createBitmap(mockImageSize, mockImageSize, Bitmap.Config.ARGB_8888);
        bitmap.setDensity(DisplayMetrics.DENSITY_DEFAULT);
        TabUtils.setDrawableAndUpdateImageMatrix(
                thumbnailView,
                new BitmapDrawable(bitmap),
                new Size(mockTargetSize, mockTargetSize));

        assertNotEquals("The bitmap image density should not be zero.", 0, bitmap.getDensity());
        assertEquals(
                "The bitmap image's density should not be scaled up on non-automotive"
                        + " devices.",
                DisplayMetrics.DENSITY_DEFAULT,
                bitmap.getDensity());
    }

    @Test
    public void testUpdateThumbnailMatrix_onAutomotiveDevice_thumbnailImageHasScaledUpDensity() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        int mockImageSize = 100;
        int mockTargetSize = 50;

        TabThumbnailView thumbnailView = Mockito.mock(TabThumbnailView.class);
        doReturn(ContextUtils.getApplicationContext()).when(thumbnailView).getContext();

        Bitmap bitmap = Bitmap.createBitmap(mockImageSize, mockImageSize, Bitmap.Config.ARGB_8888);
        bitmap.setDensity(DisplayMetrics.DENSITY_DEFAULT);
        TabUtils.setDrawableAndUpdateImageMatrix(
                thumbnailView,
                new BitmapDrawable(bitmap),
                new Size(mockTargetSize, mockTargetSize));

        assertNotEquals("The bitmap image density should not be zero.", 0, bitmap.getDensity());
        assertEquals(
                "The bitmap image's density should be scaled up on automotive.",
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_DEFAULT),
                bitmap.getDensity());
    }

    private void callAndVerifyGetTabThumbnailAspectRatio(
            Activity spyActivity,
            int horizontalAutomotiveToolbarHeightDp,
            int verticalAutomotiveToolbarWidthDp) {
        doReturn(0).when(mBrowserControlsStateProvider).getTopControlsHeight();
        float expectedAspectRatio =
                (TEST_SCREEN_WIDTH * 1.f - verticalAutomotiveToolbarWidthDp)
                        / (TEST_SCREEN_HEIGHT * 1.f
                                - horizontalAutomotiveToolbarHeightDp
                                - TEST_STATUS_BAR_HEIGHT
                                - TEST_NAVIGATION_BAR_HEIGHT);
        assertEquals(
                "Thumbnail aspect ratio is not as expected.",
                expectedAspectRatio,
                TabUtils.getTabThumbnailAspectRatio(spyActivity, mBrowserControlsStateProvider),
                0.01);
    }
}
