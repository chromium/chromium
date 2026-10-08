// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.junit.Assert.assertEquals;
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
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;

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
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowContextImpl;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.customtabs.PopupCreator;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.RenderFrameHost;
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
    ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API
})
public class ActivityTabWebContentsDelegateAndroidUnitTest {
    static class TestActivityTabWebContentsDelegateAndroid
            extends ActivityTabWebContentsDelegateAndroid {
        private final TabModel mTabModel;
        private Map<WebContents, Tab> mTabMap;
        private boolean mIsPopup;
        private boolean mIsDocumentPictureInPictureEnabled;

        // Mockito.mock() returns raw Supplier; pass through to parameterized super ctor.
        @SuppressWarnings("unchecked")
        public TestActivityTabWebContentsDelegateAndroid(
                Tab tab,
                Activity activity,
                TabCreatorManager tabCreatorManager,
                TabModel tabModel,
                ExclusiveAccessManager exclusiveAccessManager,
                FullscreenManager fullscreenManager) {
            super(
                    tab,
                    activity,
                    null,
                    false,
                    null,
                    fullscreenManager,
                    tabCreatorManager,
                    mock(Supplier.class),
                    mock(Supplier.class),
                    mock(Supplier.class),
                    mock(Supplier.class),
                    exclusiveAccessManager);
            mTabModel = tabModel;
            mTabMap = new HashMap<>();
        }

        int getDisplayModeCheckedForTesting() {
            return getDisplayModeChecked();
        }

        @Override
        protected @Nullable Tab fromWebContents(WebContents webContents) {
            return mTabMap.get(webContents);
        }

        @Override
        protected TabModel getTabModel(Tab tab) {
            return mTabModel;
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

    @Mock Profile mProfile;
    @Mock WebContents mWebContents;
    @Mock WebContents mNewWebContents;
    @Mock Tab mTab;
    @Mock TabCreatorManager mTabCreatorManager;
    @Mock TabCreator mTabCreator;
    @Mock TabModel mTabModel;
    @Mock ActivityManager mActivityManager;
    @Mock DisplayAndroid mDisplayAndroid;
    @Mock DisplayAndroidManager mDisplayAndroidManager;
    @Mock AndroidTaskUtils.MoveTaskDelegate mMoveTaskDelegate;
    @Mock AppTask mAppTask;
    @Mock PopupCreator mPopupCreator;
    @Mock MultiWindowUtils mMultiWindowUtils;
    @Mock ExclusiveAccessManager mExclusiveAccessManager;
    @Mock FullscreenManager mFullscreenManager;
    @Mock RenderFrameHost mRenderFrameHost;
    @Mock TabWindowManager mTabWindowManager;

    @Captor private ArgumentCaptor<CompletableFuture<Boolean>> mFutureCaptor;

    private static final GURL URL_1 = new GURL("https://url1.com");
    private static final GURL TARGET_URL = new GURL("https://foo.com");

    private static final int TEST_DISPLAY_ID = 73;
    private static final float TEST_DENSITY = 1.0f;
    private static final Rect TEST_BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final Rect TEST_LOCAL_BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final int TEST_TASK_ID = 123;

    private Activity mActivity;
    private FrameLayout mContentView;
    private TestActivityTabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    /** An Activity with a fixed task id. */
    private static class TaskIdActivity extends Activity {
        @Override
        public int getTaskId() {
            return TEST_TASK_ID;
        }
    }

    /** A ChromeTabbedActivity with a fixed task id. */
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        @Override
        public int getTaskId() {
            return TEST_TASK_ID;
        }
    }

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TaskIdActivity.class).setup().get();
        mContentView = new FrameLayout(mActivity);
        mActivity.setContentView(mContentView);
        MultiWindowUtils.setInstanceForTesting(mMultiWindowUtils);
        PopupCreatorFactory.setInstanceForTesting(mPopupCreator);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.getIdForWindow(any()))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);
        mTabWebContentsDelegateAndroid =
                new TestActivityTabWebContentsDelegateAndroid(
                        mTab,
                        mActivity,
                        mTabCreatorManager,
                        mTabModel,
                        mExclusiveAccessManager,
                        mFullscreenManager);
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);
        AndroidTaskUtils.setMoveTaskDelegateForTesting(mMoveTaskDelegate);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getVisibleUrl()).thenReturn(URL_1);
        when(mTabCreatorManager.getTabCreator(anyBoolean())).thenReturn(mTabCreator);
        ShadowContextImpl shadowContext = Shadow.extract(mActivity.getBaseContext());
        shadowContext.setSystemService(Context.ACTIVITY_SERVICE, mActivityManager);

        when(mDisplayAndroid.getDisplayId()).thenReturn(TEST_DISPLAY_ID);
        when(mDisplayAndroid.getDipScale()).thenReturn(TEST_DENSITY);
        when(mDisplayAndroid.getBounds()).thenReturn(TEST_BOUNDS);
        when(mDisplayAndroid.getLocalBounds()).thenReturn(TEST_LOCAL_BOUNDS);

        when(mDisplayAndroidManager.getDisplayMatching(any())).thenReturn(mDisplayAndroid);
    }

    /**
     * Adds {@code view} with the given id to the Activity's content view. The view is wrapped in a
     * container whose visibility determines {@code view.isShown()}, while the view itself stays
     * visible, so an unexpected requestFocus() on a hidden view would still succeed and be caught.
     */
    private <T extends View> T addToContentView(T view, @IdRes int id, boolean shown) {
        view.setId(id);
        FrameLayout container = new FrameLayout(mActivity);
        container.setVisibility(shown ? View.VISIBLE : View.GONE);
        container.addView(view);
        mContentView.addView(container);
        return view;
    }

    private View addFocusableView(@IdRes int id, boolean shown, boolean focusable) {
        View view = new View(mActivity);
        view.setFocusable(focusable);
        view.setFocusableInTouchMode(focusable);
        return addToContentView(view, id, shown);
    }

    /** Adds a tab sharing toolbar containing two items. */
    private ViewGroup addTabSharingToolbar(boolean shown, boolean hasFocusableItems) {
        LinearLayout toolbar = new LinearLayout(mActivity);
        for (int i = 0; i < 2; i++) {
            View item = new View(mActivity);
            item.setFocusable(hasFocusableItems);
            item.setFocusableInTouchMode(hasFocusableItems);
            toolbar.addView(item);
        }
        return addToContentView(toolbar, R.id.tab_sharing_toolbar_container, shown);
    }

    private void setRepositionPermission(boolean granted) {
        android.app.Application app = RuntimeEnvironment.getApplication();
        org.robolectric.shadows.ShadowApplication shadowApp = Shadows.shadowOf(app);
        if (granted) {
            shadowApp.grantPermissions("android.permission.REPOSITION_SELF_WINDOWS");
        } else {
            shadowApp.denyPermissions("android.permission.REPOSITION_SELF_WINDOWS");
        }
    }

    @Test
    public void testIsDocumentPictureInPictureBlockedBySystem() {
        setRepositionPermission(true);
        // Test in app fullscreen (not multi-window mode) -> Blocked.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        assertTrue(mTabWebContentsDelegateAndroid.isDocumentPictureInPictureBlockedBySystem());

        // Test in multi-window mode -> Not blocked.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(true);
        assertFalse(mTabWebContentsDelegateAndroid.isDocumentPictureInPictureBlockedBySystem());
    }

    @Test
    public void
            testIsDocumentPictureInPictureBlockedBySystem_BlockedWhenCurrentBrowserNotDefault() {
        // Test when the current browser is NOT the default browser -> Blocked even in multi-window
        // mode.
        setRepositionPermission(false);
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(true);
        assertTrue(mTabWebContentsDelegateAndroid.isDocumentPictureInPictureBlockedBySystem());
    }

    @Test
    public void testAddNewContentsNotInTabGroup() {
        Map<WebContents, Tab> tabMap =
                Map.of(mWebContents, mock(Tab.class), mNewWebContents, mock(Tab.class));
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false,
                null);
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
    }

    @Test
    public void testAddNewContentsToTabGroup() {
        Tab parentTab = mock(Tab.class);
        Tab newTab = mock(Tab.class);
        when(parentTab.getTabGroupId()).thenReturn(Token.createRandom());
        when(mTabCreator.createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any()))
                .thenReturn(newTab);
        when(mTabModel.isTabInTabGroup(any())).thenReturn(true);
        when(mTabModel.isTabStateInitialized()).thenReturn(true);
        Map<WebContents, Tab> tabMap = Map.of(mWebContents, parentTab, mNewWebContents, newTab);
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false,
                null);
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        Arrays.asList(newTab),
                        parentTab,
                        TabGroupMergeNotificationType.DONT_NOTIFY);
    }

    @Test
    public void testAddNewContents_NewBackgroundTab_NotInTabGroup() {
        when(mTab.getTabGroupId()).thenReturn(null);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_BACKGROUND_TAB,
                new WindowFeatures(),
                false,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND),
                        any());
    }

    @Test
    public void testAddNewContents_NewBackgroundTab_InTabGroup() {
        when(mTab.getTabGroupId()).thenReturn(Token.createRandom());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_BACKGROUND_TAB,
                new WindowFeatures(),
                false,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP),
                        any());
    }

    @Test
    public void testAddNewContents_NewForegroundTab_NotInTabGroup() {
        when(mTab.getTabGroupId()).thenReturn(null);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_FOREGROUND),
                        any());
    }

    @Test
    public void testAddNewContents_NewForegroundTab_InTabGroup() {
        when(mTab.getTabGroupId()).thenReturn(Token.createRandom());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP),
                        any());
    }

    @Test
    public void testAddNewContents_NewPopup_InTabGroup_DoesNotUseInGroupLaunchType() {
        when(mTab.getTabGroupId()).thenReturn(Token.createRandom());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_FOREGROUND),
                        any());
    }

    @Test
    public void testAddNewContents_NewWindow_InTabGroup_DoesNotUseInGroupLaunchType() {
        when(mTab.getTabGroupId()).thenReturn(Token.createRandom());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_WINDOW,
                new WindowFeatures(),
                false,
                null);

        verify(mTabCreator)
                .createTabWithWebContents(
                        eq(mTab),
                        eq(false),
                        eq(mNewWebContents),
                        eq(TabLaunchType.FROM_LONGPRESS_FOREGROUND),
                        any());
    }

    @Test
    public void testAddNewContents_InTabGroup_AlreadyGrouped_DoesNotReMerge() {
        Tab parentTab = mock(Tab.class);
        Tab newTab = mock(Tab.class);
        Token tabGroupId = Token.createRandom();
        when(parentTab.getTabGroupId()).thenReturn(tabGroupId);
        when(newTab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabCreator.createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any()))
                .thenReturn(newTab);
        when(mTabModel.isTabInTabGroup(any())).thenReturn(true);
        when(mTabModel.isTabStateInitialized()).thenReturn(true);
        Map<WebContents, Tab> tabMap = Map.of(mWebContents, parentTab, mNewWebContents, newTab);
        mTabWebContentsDelegateAndroid.setTabMap(tabMap);

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                new WindowFeatures(),
                false,
                null);
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
    }

    @Test
    public void testAddNewContentsDoesNotAddToTabModelWhenMovingTabToPopupIsSuccessful() {
        when(mPopupCreator.moveTabToNewPopup(any(), any())).thenReturn(true);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), mFutureCaptor.capture());
        CompletableFuture<Boolean> capturedFuture = mFutureCaptor.getValue();
        assertTrue(
                "The final decision to add the tab to the TabModel should have already been made",
                capturedFuture.isDone());
        assertFalse(
                "The final decision to add the tab to the TabModel should be negative",
                capturedFuture.getNow(null));
    }

    @Test
    public void testAddNewContentsAddToTabModelWhenMovingTabToPopupIsUnsuccessful() {
        when(mPopupCreator.moveTabToNewPopup(any(), any())).thenReturn(false);
        Tab newTab = mock(Tab.class);
        doReturn(newTab)
                .when(mTabCreator)
                .createTabWithWebContents(any(), anyBoolean(), any(), anyInt(), any());

        mTabWebContentsDelegateAndroid.addNewContents(
                mWebContents,
                mNewWebContents,
                TARGET_URL,
                WindowOpenDisposition.NEW_POPUP,
                new WindowFeatures(),
                true,
                null);

        verify(mTabCreator, times(1))
                .createTabWithWebContents(
                        any(), anyBoolean(), any(), anyInt(), mFutureCaptor.capture());
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
        mTabWebContentsDelegateAndroid.bringActivityToForeground();

        verify(mActivityManager).moveTaskToFront(TEST_TASK_ID, 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_ACTIVITY_MANAGER_FOR_TAB_ACTIVATION)
    public void testBringActivityToForeground_LaunchesIntentInInstance() {
        int windowId = 1;
        int testTabId = 42;
        when(mTab.getId()).thenReturn(testTabId);

        ChromeTabbedActivity tabbedActivity = new TestChromeTabbedActivity();
        when(mTabWindowManager.getIdForWindow(tabbedActivity)).thenReturn(windowId);
        MultiWindowUtils.setActivityByWindowIdForTesting(windowId, tabbedActivity);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        TestActivityTabWebContentsDelegateAndroid delegate =
                new TestActivityTabWebContentsDelegateAndroid(
                        mTab,
                        tabbedActivity,
                        mTabCreatorManager,
                        mTabModel,
                        mExclusiveAccessManager,
                        mFullscreenManager);

        delegate.bringActivityToForeground();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mAppTask).startActivity(any(), intentCaptor.capture(), eq(null));
        assertEquals(testTabId, IntentHandler.getBringTabToFrontId(intentCaptor.getValue()));
        assertEquals(0, intentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        verify(mActivityManager, never()).moveTaskToFront(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DOCUMENT_PICTURE_IN_PICTURE_API)
    public void testAddNewContents_DocumentPictureInPicture_Enabled() {
        mTabWebContentsDelegateAndroid.setIsDocumentPictureInPictureEnabled(true);
        when(mPopupCreator.moveWebContentsToNewDocumentPictureInPictureWindow(any(), any(), any()))
                .thenReturn(true);

        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        mNewWebContents,
                        TARGET_URL,
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

        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        mNewWebContents,
                        TARGET_URL,
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

        PictureInPictureWindowOptions options =
                new PictureInPictureWindowOptions(new Rect(0, 0, 100, 100), false);

        boolean result =
                mTabWebContentsDelegateAndroid.addNewContents(
                        mWebContents,
                        mNewWebContents,
                        TARGET_URL,
                        WindowOpenDisposition.NEW_PICTURE_IN_PICTURE,
                        new WindowFeatures(),
                        true,
                        options);

        assertFalse(result);
    }

    @Test
    public void testSetContentsBoundsClampsBounds() {
        mTabWebContentsDelegateAndroid.setIsPopup(true);
        mTabWebContentsDelegateAndroid.setContentsBounds(
                mWebContents, new Rect(-100, -100, 2000, 2000));

        ArgumentCaptor<Rect> captor = ArgumentCaptor.forClass(Rect.class);
        verify(mMoveTaskDelegate).moveTaskTo(any(), eq(TEST_DISPLAY_ID), captor.capture());
        final Rect passedBounds = captor.getValue();
        Assert.assertTrue(
                "The bounds passed to moveTaskTo do not fit inside display",
                TEST_LOCAL_BOUNDS.contains(passedBounds));
    }

    @Test
    public void testSetContentsBoundsNoOpIfNotPopup() {
        mTabWebContentsDelegateAndroid.setIsPopup(false);

        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));

        verify(mMoveTaskDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    @Test
    public void testSetContentsBoundsNoOpIfNoDisplayMatching() {
        doReturn(null).when(mDisplayAndroidManager).getDisplayMatching(any());

        mTabWebContentsDelegateAndroid.setIsPopup(true);
        mTabWebContentsDelegateAndroid.setContentsBounds(mWebContents, new Rect(0, 0, 400, 400));

        verify(mMoveTaskDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    @Test
    public void testCanEnterFullscreenModeForTab() {
        when(mExclusiveAccessManager.canEnterFullscreenModeForTab(mRenderFrameHost))
                .thenReturn(true);
        assertTrue(mTabWebContentsDelegateAndroid.canEnterFullscreenModeForTab(mRenderFrameHost));
        verify(mExclusiveAccessManager, times(1)).canEnterFullscreenModeForTab(mRenderFrameHost);

        when(mExclusiveAccessManager.canEnterFullscreenModeForTab(mRenderFrameHost))
                .thenReturn(false);
        assertFalse(mTabWebContentsDelegateAndroid.canEnterFullscreenModeForTab(mRenderFrameHost));
    }

    @Test
    public void testTakeFocus_forward() {
        View urlBar = addFocusableView(R.id.url_bar, /* shown= */ true, /* focusable= */ true);

        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ false));
        assertTrue(urlBar.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_menuButton() {
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ true, /* focusable= */ true);

        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        assertTrue(menuButton.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_tabSharingToolbar() {
        ViewGroup tabSharingToolbar =
                addTabSharingToolbar(/* shown= */ true, /* hasFocusableItems= */ true);
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ true, /* focusable= */ true);

        // The tab sharing toolbar sits below the browser toolbar, so it must take focus first.
        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        // FOCUS_BACKWARD lands on the toolbar's last focusable item.
        assertTrue(tabSharingToolbar.getChildAt(1).isFocused());
        assertFalse(menuButton.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_tabSharingToolbarHidden() {
        ViewGroup tabSharingToolbar =
                addTabSharingToolbar(/* shown= */ false, /* hasFocusableItems= */ true);
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ true, /* focusable= */ true);

        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        assertFalse(tabSharingToolbar.hasFocus());
        assertTrue(menuButton.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_tabSharingToolbarNotFocusable() {
        ViewGroup tabSharingToolbar =
                addTabSharingToolbar(/* shown= */ true, /* hasFocusableItems= */ false);
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ true, /* focusable= */ true);

        // If the toolbar has no focusable descendant, focus continues up to the browser toolbar.
        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        assertFalse(tabSharingToolbar.hasFocus());
        assertTrue(menuButton.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_tabSwitcherButton() {
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ false, /* focusable= */ true);
        View tabSwitcherButton =
                addFocusableView(
                        R.id.tab_switcher_button, /* shown= */ true, /* focusable= */ true);

        assertTrue(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        assertFalse(menuButton.isFocused());
        assertTrue(tabSwitcherButton.isFocused());
    }

    @Test
    public void testTakeFocus_reverse_buttonsHidden() {
        View menuButton =
                addFocusableView(R.id.menu_button, /* shown= */ false, /* focusable= */ true);
        View tabSwitcherButton =
                addFocusableView(
                        R.id.tab_switcher_button, /* shown= */ false, /* focusable= */ true);

        assertFalse(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
        assertFalse(menuButton.isFocused());
        assertFalse(tabSwitcherButton.isFocused());
    }

    @Test
    public void testTakeFocus_forward_requestFocusFails() {
        View urlBar = addFocusableView(R.id.url_bar, /* shown= */ true, /* focusable= */ false);

        assertFalse(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ false));
        assertFalse(urlBar.isFocused());
    }

    @Test
    public void testTakeFocus_nullViews() {
        // No views are added to the content view, so findViewById() returns null for all ids.
        assertFalse(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ false));
        assertFalse(mTabWebContentsDelegateAndroid.takeFocus(/* reverse= */ true));
    }

    @Test
    public void testGetDisplayModeChecked_fullscreen() {
        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(true);
        assertEquals(
                DisplayMode.FULLSCREEN,
                mTabWebContentsDelegateAndroid.getDisplayModeCheckedForTesting());

        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(false);
        assertEquals(
                DisplayMode.BROWSER,
                mTabWebContentsDelegateAndroid.getDisplayModeCheckedForTesting());
    }

    @Test
    public void testHandleKeyboardEvent_escapeStopsLoadingWhenRepeatCountZero() {
        when(mWebContents.isLoading()).thenReturn(true);
        KeyEvent escapeEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);

        mTabWebContentsDelegateAndroid.handleKeyboardEvent(escapeEvent);

        verify(mWebContents).stop();
    }

    @Test
    public void testHandleKeyboardEvent_escapeIgnoredWhenRepeatCountNonZero() {
        when(mWebContents.isLoading()).thenReturn(true);
        KeyEvent escapeRepeatEvent =
                new KeyEvent(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_ESCAPE,
                        /* repeat= */ 1);

        mTabWebContentsDelegateAndroid.handleKeyboardEvent(escapeRepeatEvent);

        verify(mWebContents, never()).stop();
    }

    @Test
    public void testHandleKeyboardEvent_escapeIgnoredWhenNotLoading() {
        when(mWebContents.isLoading()).thenReturn(false);
        KeyEvent escapeEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);

        mTabWebContentsDelegateAndroid.handleKeyboardEvent(escapeEvent);

        verify(mWebContents, never()).stop();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testIsPictureInPictureEnabled_suppressedWhenAndroidAutoProjected() {
        Shadows.shadowOf(mActivity.getPackageManager())
                .setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);

        BrowserUiUtils.setIsAndroidAutoProjectedForTesting(false);
        assertTrue(mTabWebContentsDelegateAndroid.isPictureInPictureEnabled());

        BrowserUiUtils.setIsAndroidAutoProjectedForTesting(true);
        assertFalse(mTabWebContentsDelegateAndroid.isPictureInPictureEnabled());
    }

    /** Web Picture-in-Picture requires Android R (b/143784148), unlike fullscreen video PiP. */
    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testIsPictureInPictureEnabled_suppressedBeforeAndroidR() {
        Shadows.shadowOf(mActivity.getPackageManager())
                .setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);
        BrowserUiUtils.setIsAndroidAutoProjectedForTesting(false);

        assertFalse(mTabWebContentsDelegateAndroid.isPictureInPictureEnabled());
    }
}
