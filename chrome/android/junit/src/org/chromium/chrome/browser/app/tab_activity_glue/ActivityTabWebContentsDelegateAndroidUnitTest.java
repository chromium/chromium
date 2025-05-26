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
import android.content.Context;

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

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
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
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.shadows.ShadowColorUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** Unit test for {@link ActivityTabWebContentsDelegateAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowColorUtils.class, ShadowWebContentsDarkModeController.class})
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
@DisableFeatures({
    ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE,
    ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN
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
                    mock(Supplier.class));
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

    GURL mUrl1 = new GURL("https://url1.com");
    GURL mUrl2 = new GURL("https://url2.com");

    private TestActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Before
    public void setup() {
        mTabWebContentsDelegateAndroid =
                new TestActivityTabWebContentsDelegateAndroid(
                        mTab, mActivity, mTabCreatorManager, mTabGroupModelFilter);

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mUrl1).when(mWebContents).getVisibleUrl();
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        when(mActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
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

    @Test
    @EnableFeatures(ChromeFeatureList.GROUP_NEW_TAB_WITH_PARENT)
    public void testAddNewContentsNotInTabGroup() {
        WebContents newWebContents = mock(WebContents.class);
        Map<WebContents, Tab> tabMap =
                Map.of(mWebContents, mock(Tab.class), newWebContents, mock(Tab.class));
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.webContentsCreated(
                mWebContents, 0, 0, "testFrame", new GURL("https://foo.com"), newWebContents);
        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false);
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyBoolean());
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
                .createTabWithWebContents(any(), any(), anyInt(), any(), anyBoolean());
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(any());
        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();
        Map<WebContents, Tab> tabMap = Map.of(mWebContents, parentTab, newWebContents, newTab);
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.webContentsCreated(
                mWebContents, 0, 0, "testFrame", new GURL("https://foo.com"), newWebContents);
        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false);
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(Arrays.asList(newTab), parentTab, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testAddNewContentsAddToTabModelWhenPopupsNotEnabled() {
        PopupCreator.setArePopupsEnabledForTesting(false);
        WebContents newWebContents = mock(WebContents.class);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), any(), anyInt(), any(), anyBoolean());

        mTabWebContentsDelegateAndroid.webContentsCreated(
                mWebContents, 0, 0, "testFrame", new GURL("https://foo.com"), newWebContents);
        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(any(), any(), anyInt(), any(), eq(true));
        verify(mTabCreator, never())
                .createTabWithWebContents(any(), any(), anyInt(), any(), eq(false));
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

    private void assertForceDarkEnabledForWebContents(boolean isEnabled) {
        Assert.assertEquals(
                "Value of #isForceDarkWebContentEnabled is different than test settings.",
                isEnabled,
                mTabWebContentsDelegateAndroid.isForceDarkWebContentEnabled());
    }
}
