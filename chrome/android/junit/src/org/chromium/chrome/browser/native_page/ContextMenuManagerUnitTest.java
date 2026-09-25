// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPopupWindow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ContextMenuManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowPopupWindow.class)
public class ContextMenuManagerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ContextMenuManager mManager;
    private View mAnchorView;

    @Mock NativePageNavigationDelegate mNavigationDelegate;
    @Mock TouchEnabledDelegate mTouchEnabledDelegate;
    @Mock ContextMenuManager.Delegate mDelegate;
    @Mock Clipboard mClipboard;
    @Mock Profile mProfile;
    @Mock NativePageHost mHost;
    @Mock TabModelSelector mTabModelSelector;
    @Mock Tab mTab;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mAnchorView = spy(new View(mActivity, null));
        mManager = new ContextMenuManager(mNavigationDelegate, mTouchEnabledDelegate, () -> {}, "");
    }

    @Test
    public void emptyListContextMenu() {
        assertFalse(
                "showContextMenu failed since list is empty.",
                mManager.showListContextMenu(mAnchorView, mDelegate));
    }

    @Test
    public void showListContextMenu() {
        doReturn(true).when(mDelegate).isItemSupported(anyInt());
        doReturn(false).when(mNavigationDelegate).isOpenInNewTabInGroupEnabled();
        doReturn(false).when(mNavigationDelegate).isOpenInOtherWindowEnabled();
        doReturn(false).when(mNavigationDelegate).isOpenInIncognitoEnabled();
        doReturn(null).when(mDelegate).getUrl();
        doReturn(true).when(mAnchorView).isAttachedToWindow();
        // Disable navigation to new window.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);

        assertTrue(
                "showContextMenu failed since list is empty.",
                mManager.showListContextMenu(mAnchorView, mDelegate));
        assertNotNull("List context menu is null.", mManager.getListMenuForTesting());
        verify(mDelegate).onContextMenuCreated();
    }

    @Test
    public void testHandleMenuItemClick_MoveUp() {
        mManager.handleMenuItemClick(ContextMenuItemId.MOVE_UP, mDelegate);
        verify(mDelegate).moveItemUp();
        verify(mDelegate, never()).moveItemDown();
    }

    @Test
    public void testHandleMenuItemClick_MoveDown() {
        mManager.handleMenuItemClick(ContextMenuItemId.MOVE_DOWN, mDelegate);
        verify(mDelegate).moveItemDown();
        verify(mDelegate, never()).moveItemUp();
    }

    @Test
    public void testShouldShowItem_MoveUp() {
        doReturn(true).when(mDelegate).isItemSupported(ContextMenuItemId.MOVE_UP);
        AccessibilityStateTestHelper.setIsAnyAccessibilityServiceEnabledForTesting(true);
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.MOVE_UP, mDelegate));
        AccessibilityStateTestHelper.setIsAnyAccessibilityServiceEnabledForTesting(false);
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.MOVE_UP, mDelegate));
    }

    @Test
    public void testShouldShowItem_MoveDown() {
        doReturn(true).when(mDelegate).isItemSupported(ContextMenuItemId.MOVE_DOWN);
        AccessibilityStateTestHelper.setIsAnyAccessibilityServiceEnabledForTesting(true);
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.MOVE_DOWN, mDelegate));
        AccessibilityStateTestHelper.setIsAnyAccessibilityServiceEnabledForTesting(false);
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.MOVE_DOWN, mDelegate));
    }

    @Test
    public void testShouldShowItem_CopyLinkAddress() {
        doReturn(true).when(mDelegate).isItemSupported(ContextMenuItemId.COPY_LINK_ADDRESS);
        doReturn(JUnitTestGURLs.URL_1).when(mDelegate).getUrl();
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.COPY_LINK_ADDRESS, mDelegate));

        doReturn(GURL.emptyGURL()).when(mDelegate).getUrl();
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.COPY_LINK_ADDRESS, mDelegate));

        doReturn(null).when(mDelegate).getUrl();
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.COPY_LINK_ADDRESS, mDelegate));
    }

    @Test
    public void testHandleMenuItemClick_CopyLinkAddress() {
        Clipboard.setInstanceForTesting(mClipboard);
        GURL url = JUnitTestGURLs.URL_1;
        doReturn(url).when(mDelegate).getUrl();

        assertTrue(mManager.handleMenuItemClick(ContextMenuItemId.COPY_LINK_ADDRESS, mDelegate));
        verify(mClipboard).copyUrlToClipboard(url);
    }

    @Test
    public void testIncognitoNavigationDelegate() {
        doReturn(true).when(mProfile).isOffTheRecord();
        NativePageNavigationDelegateImpl delegate =
                new NativePageNavigationDelegateImpl(
                        mActivity, mProfile, mHost, mTabModelSelector, mTab);

        assertFalse(delegate.isOpenInIncognitoEnabled());

        LoadUrlParams params = new LoadUrlParams(JUnitTestGURLs.URL_1.getSpec());
        delegate.openUrl(WindowOpenDisposition.CURRENT_TAB, params);
        verify(mHost).loadUrl(params, /* incognito= */ true);

        delegate.openUrl(WindowOpenDisposition.NEW_BACKGROUND_TAB, params);
        verify(mTabModelSelector)
                .openNewTab(
                        params,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        mTab,
                        /* incognito= */ true);

        delegate.openUrlInGroup(WindowOpenDisposition.NEW_BACKGROUND_TAB, params);
        verify(mTabModelSelector)
                .openNewTab(
                        params,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        mTab,
                        /* incognito= */ true);
    }

    @Test
    public void testShouldShowItem_MultiWindow() {
        doReturn(true).when(mDelegate).isItemSupported(anyInt());

        // New window supported.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_NEW_WINDOW, mDelegate));

        // New window not supported.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_NEW_WINDOW, mDelegate));

        // Other window supported.
        doReturn(true).when(mNavigationDelegate).isOpenInOtherWindowEnabled();
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_OTHER_WINDOW, mDelegate));

        // Other window not supported.
        doReturn(false).when(mNavigationDelegate).isOpenInOtherWindowEnabled();
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_OTHER_WINDOW, mDelegate));

        // Incognito window supported.
        doReturn(true).when(mNavigationDelegate).isOpenInIncognitoEnabled();
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_INCOGNITO_WINDOW, mDelegate));
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, mDelegate));

        // Incognito window not supported.
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_INCOGNITO_WINDOW, mDelegate));
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, mDelegate));
    }
}
