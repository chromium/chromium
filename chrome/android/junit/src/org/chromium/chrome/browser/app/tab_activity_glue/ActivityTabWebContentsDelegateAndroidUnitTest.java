// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.PopupCreator;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
@DisableFeatures({
    ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE,
    ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN,
    ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API
})
public class ActivityTabWebContentsDelegateAndroidUnitTest {
    static class TestActivityTabWebContentsDelegateAndroid
            extends ActivityTabWebContentsDelegateAndroid {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private Map<WebContents, Tab> mTabMap;
        private boolean mIsPopup;
        private boolean mIsDocumentPictureInPictureEnabled;

        // Mockito.mock() returns raw Supplier; pass through to parameterized super ctor.
        @SuppressWarnings("unchecked")
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

        @Override
        protected boolean isDocumentPictureInPictureEnabled() {
            return mIsDocumentPictureInPictureEnabled;
        }

        public void setIsDocumentPictureInPictureEnabled(
                boolean isDocumentPictureInPictureEnabled) {
            mIsDocumentPictureInPictureEnabled = isDocumentPictureInPictureEnabled;
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
    @Mock PopupCreator mPopupCreator;
    @Mock MultiWindowUtils mMultiWindowUtils;

    @Captor private ArgumentCaptor<CompletableFuture<Boolean>> mFutureCaptor;

    GURL mUrl1 = new GURL("https://url1.com");
    GURL mUrl2 = new GURL("https://url2.com");

    private static final int TEST_DISPLAY_ID = 73;
    private static final float TEST_DENSITY = 1.0f;
    private static final Rect TEST_BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final Rect TEST_LOCAL_BOUNDS = new Rect(0, 0, 1920, 1080);

    private TestActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Before
    public void setup() {
        MultiWindowUtils.setInstanceForTesting(mMultiWindowUtils);
        PopupCreatorFactory.setInstanceForTesting(mPopupCreator);
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

    @Test
    public void testIsDocumentPictureInPictureBlockedBySystem() {
        // Test in app fullscreen (not multi-window mode) -> Blocked.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        assertTrue(mTabWebContentsDelegateAndroid.isDocumentPictureInPictureBlockedBySystem());

        // Test in multi-window mode -> Not blocked.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(true);
        assertFalse(mTabWebContentsDelegateAndroid.isDocumentPictureInPictureBlockedBySystem());
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
                false,
                null);
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
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), any());
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
                false,
                null);
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        Arrays.asList(newTab), parentTab, MergeNotificationType.DONT_NOTIFY);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testAddNewContentsAddToTabModelWhenPopupFlagNotEnabled() {
        WebContents newWebContents = mock(WebContents.class);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), any());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), any(), mFutureCaptor.capture());
        CompletableFuture<Boolean> capturedFuture = mFutureCaptor.getValue();
        assertTrue(
                "The final decision to add the tab to the TabModel should have already been made",
                capturedFuture.isDone());
        assertTrue(
                "The final decision to add the tab to the TabModel should be positive",
                capturedFuture.getNow(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testAddNewContentsDoesNotAddToTabModelWhenMovingTabToPopupIsSuccessful() {
        when(mPopupCreator.moveTabToNewPopup(any(), any())).thenReturn(true);
        WebContents newWebContents = mock(WebContents.class);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), any());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), any(), mFutureCaptor.capture());
        CompletableFuture<Boolean> capturedFuture = mFutureCaptor.getValue();
        assertTrue(
                "The final decision to add the tab to the TabModel should have already been made",
                capturedFuture.isDone());
        assertFalse(
                "The final decision to add the tab to the TabModel should be negative",
                capturedFuture.getNow(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testAddNewContentsAddToTabModelWhenMovingTabToPopupIsUnsuccessful() {
        when(mPopupCreator.moveTabToNewPopup(any(), any())).thenReturn(false);
        WebContents newWebContents = mock(WebContents.class);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any(), any());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                newWebContents,
                new GURL("https://foo.com"),
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), any(), mFutureCaptor.capture());
        CompletableFuture<Boolean> capturedFuture = mFutureCaptor.getValue();
        assertTrue(
                "The final decision to add the tab to the TabModel should have already been made",
                capturedFuture.isDone());
        assertTrue(
                "The final decision to add the tab to the TabModel should be positive",
                capturedFuture.getNow(null));
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
    @EnableFeatures(ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API)
    public void testAddNewContents_DocumentPictureInPicture_Enabled() {
        mTabWebContentsDelegateAndroid.setIsDocumentPictureInPictureEnabled(true);
        when(mPopupCreator.moveWebContentsToNewDocumentPictureInPictureWindow(any(), any(), any()))
                .thenReturn(true);

        WebContents newWebContents = mock(WebContents.class);
        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        newWebContents,
                        new GURL("https://foo.com"),
                        WindowOpenDisposition.NEW_PICTURE_IN_PICTURE,
                        new WindowFeatures(),
                        true,
                        options);

        assertTrue(result);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API)
    public void testAddNewContents_DocumentPictureInPicture_Disabled() {
        mTabWebContentsDelegateAndroid.setIsDocumentPictureInPictureEnabled(false);

        WebContents newWebContents = mock(WebContents.class);
        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        newWebContents,
                        new GURL("https://foo.com"),
                        WindowOpenDisposition.NEW_PICTURE_IN_PICTURE,
                        new WindowFeatures(),
                        true,
                        options);

        assertFalse(result);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API)
    public void testAddNewContents_DocumentPictureInPicture_Enabled_LaunchFailed() {
        mTabWebContentsDelegateAndroid.setIsDocumentPictureInPictureEnabled(true);
        when(mPopupCreator.moveWebContentsToNewDocumentPictureInPictureWindow(any(), any(), any()))
                .thenReturn(false);

        WebContents newWebContents = mock(WebContents.class);
        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        newWebContents,
                        new GURL("https://foo.com"),
                        WindowOpenDisposition.NEW_PICTURE_IN_PICTURE,
                        new WindowFeatures(),
                        true,
                        options);

        assertFalse(result);
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

}
