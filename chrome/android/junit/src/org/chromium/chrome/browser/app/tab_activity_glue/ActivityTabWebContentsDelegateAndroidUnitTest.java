// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.graphics.Rect;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroidUnitTest.ShadowWebContentsDarkModeController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/** Unit test for {@link ActivityTabWebContentsDelegateAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowWebContentsDarkModeController.class})
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
@DisableFeatures({
    ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE,
    ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN,
    ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API
})
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

    static class TestActivityTabWebContentsDelegateAndroid
            extends ActivityTabWebContentsDelegateAndroid {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private Map<WebContents, Tab> mTabMap;
        private boolean mIsPopup;

        public TestActivityTabWebContentsDelegateAndroid(
                Tab tab,
                Activity activity,
                TabCreatorManager tabCreatorManager,
                TabGroupModelFilter tabGroupModelFilter) {
            super(
                    tab,
                    activity,
                    null,
                    false,
                    null,
                    null,
                    tabCreatorManager,
                    mock(Supplier.class),
                    mock(Supplier.class),
                    mock(Supplier.class),
                    null);
            mTabGroupModelFilter = tabGroupModelFilter;
            mTabMap = new HashMap<>();
        }

        @Override
        protected Tab fromWebContents(WebContents webContents) {
            return mTabMap.get(webContents);
        }

        @Override
        protected TabGroupModelFilter getTabGroupModelFilter(Tab tab) {
            return mTabGroupModelFilter;
        }

        public void setTabMap(Map<WebContents, Tab> tabMap) {
            mTabMap = tabMap;
        }

        @Override
        protected boolean isPopup() {
            return mIsPopup;
        }

        public void setIsPopup(boolean isPopup) {
            mIsPopup = isPopup;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Activity mActivity;
    @Mock Profile mProfile;
    @Mock WebContents mWebContents;
    @Mock Tab mTab;
    @Mock TabCreatorManager mTabCreatorManager;
    @Mock TabCreator mTabCreator;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock ActivityManager mActivityManager;
    @Mock AconfigFlaggedApiDelegate mFlaggedApiDelegate;
    @Mock DisplayAndroid mDisplayAndroid;
    @Mock DisplayAndroidManager mDisplayAndroidManager;
    @Mock AppTask mAppTask;

    GURL mUrl1 = new GURL("https://url1.com");
    GURL mUrl2 = new GURL("https://url2.com");

    private static final int TEST_DISPLAY_ID = 73;
    private static final float TEST_DENSITY = 1.0f;
    private static final Rect TEST_BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final Rect TEST_LOCAL_BOUNDS = new Rect(0, 0, 1920, 1080);

    private TestActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Before
    public void setup() {
        mTabWebContentsDelegateAndroid =
                new TestActivityTabWebContentsDelegateAndroid(
                        mTab, mActivity, mTabCreatorManager, mTabGroupModelFilter);
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mUrl1).when(mWebContents).getVisibleUrl();
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        when(mActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);

        doReturn(TEST_DISPLAY_ID).when(mDisplayAndroid).getDisplayId();
        doReturn(TEST_DENSITY).when(mDisplayAndroid).getDipScale();
        doReturn(TEST_BOUNDS).when(mDisplayAndroid).getBounds();
        doReturn(TEST_LOCAL_BOUNDS).when(mDisplayAndroid).getLocalBounds();

        doReturn(mDisplayAndroid).when(mDisplayAndroidManager).getDisplayMatching(any());
    }

    @After
    public void tearDown() {
        ShadowWebContentsDarkModeController.sBlockedUrl = null;
    }

    @Test
    public void testIsNightMode() {
        ColorUtils.setInNightModeForTesting(true);
        Assert.assertTrue(
                "#isNightModeEnabled is false.",
                mTabWebContentsDelegateAndroid.isNightModeEnabled());

        ColorUtils.setInNightModeForTesting(false);
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
        ColorUtils.setInNightModeForTesting(false);
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_GlobalSettingDisabled() {
        ColorUtils.setInNightModeForTesting(true);
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = false;
        assertForceDarkEnabledForWebContents(false);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_GlobalSettingEnabled() {
        ColorUtils.setInNightModeForTesting(true);
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = true;
        assertForceDarkEnabledForWebContents(true);
    }

    @Test
    public void testForceDarkWebContent_DarkTheme_DisabledForUrl() {
        ColorUtils.setInNightModeForTesting(true);
        ShadowWebContentsDarkModeController.sGlobalSettingsEnabled = true;
        ShadowWebContentsDarkModeController.sBlockedUrl = mUrl1;
        assertForceDarkEnabledForWebContents(false);

        doReturn(mUrl2).when(mWebContents).getVisibleUrl();
        assertForceDarkEnabledForWebContents(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GROUP_NEW_TAB_WITH_PARENT)
    public void testAddNewContentsNotInTabGroup() {
        WebContents newWebContents = mock(WebContents.class);
        Map<WebContents, Tab> tabMap =
                Map.of(mWebContents, mock(Tab.class), newWebContents, mock(Tab.class));
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false);
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GROUP_NEW_TAB_WITH_PARENT)
    public void testAddNewContentsToTabGroup() {
        WebContents newWebContents = mock(WebContents.class);
        Tab parentTab = mock(Tab.class);
        Tab newTab = mock(Tab.class);
        doReturn(Token.createRandom()).when(parentTab).getTabGroupId();
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), any(), anyBoolean());
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(any());
        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();
        Map<WebContents, Tab> tabMap = Map.of(mWebContents, parentTab, newWebContents, newTab);
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false);
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        Arrays.asList(newTab), parentTab, MergeNotificationType.DONT_NOTIFY);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testAddNewContentsAddToTabModelWhenPopupsNotEnabled() {
        PopupCreator.setArePopupsEnabledForTesting(false);
        WebContents newWebContents = mock(WebContents.class);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), any(), anyBoolean());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), eq(true));
        verify(mTabCreator, never())
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), eq(false));
    }

    @Test
    public void testDestroy() {
        verify(mTab).addObserver(any());
        mTabWebContentsDelegateAndroid.destroy();
        verify(mTab).removeObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_ACTIVITY_MANAGER_FOR_TAB_ACTIVATION)
    public void testBringActivityToForeground() {
        final int taskId = 123;
        when(mActivity.getTaskId()).thenReturn(taskId);

        mTabWebContentsDelegateAndroid.bringActivityToForeground();

        verify(mActivity).getSystemService(Context.ACTIVITY_SERVICE);
        verify(mActivityManager).moveTaskToFront(taskId, 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testSetContentsBoundsClampsBounds() {
        mTabWebContentsDelegateAndroid.setIsPopup(true);
        mTabWebContentsDelegateAndroid.setContentsBounds(
                mWebContents, new Rect(-100, -100, 2000, 2000));

        ArgumentCaptor<Rect> captor = ArgumentCaptor.forClass(Rect.class);
        verify(mFlaggedApiDelegate).moveTaskTo(any(), eq(TEST_DISPLAY_ID), captor.capture());
        final Rect passedBounds = captor.getValue();
        Assert.assertTrue(
                "The bounds passed to moveTaskTo do not fit inside display",
                TEST_LOCAL_BOUNDS.contains(passedBounds));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testSetContentsBoundsNoOpIfFlagDisabled() {
        mTabWebContentsDelegateAndroid.setIsPopup(true);

        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));

        verify(mFlaggedApiDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testSetContentsBoundsNoOpIfDelegateNull() {
        mTabWebContentsDelegateAndroid.setIsPopup(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);

        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));
        // No assertions -- just verifying that there is no NPE thrown.
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testSetContentsBoundsNoOpIfNotPopup() {
        mTabWebContentsDelegateAndroid.setIsPopup(false);

        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));

        verify(mFlaggedApiDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testSetContentsBoundsNoOpIfNoDisplayMatching() {
        doReturn(null).when(mDisplayAndroidManager).getDisplayMatching(any());

        mTabWebContentsDelegateAndroid.setIsPopup(true);
        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));

        verify(mFlaggedApiDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    private void assertForceDarkEnabledForWebContents(boolean isEnabled) {
        Assert.assertEquals(
                "Value of #isForceDarkWebContentEnabled is different than test settings.",
                isEnabled,
                mTabWebContentsDelegateAndroid.isForceDarkWebContentEnabled());
    }
}
