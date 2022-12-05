// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorBookmarkAction.TabSelectionEditorBookmarkActionDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link TabSelectionEditorBookmarkAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorBookmarkActionUnitTest {
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock
    private ActionDelegate mDelegate;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private TabSelectionEditorBookmarkActionDelegate mTabSelectionEditorBookmarkActionDelegate;
    private MockTabModel mTabModel;
    private TabSelectionEditorBookmarkAction mAction;
    private Activity mActivity;

    @Captor
    ArgumentCaptor<Activity> mActivityCaptor;
    @Captor
    ArgumentCaptor<List<Tab>> mTabsCaptor;
    @Captor
    ArgumentCaptor<SnackbarManager> mSnackbarManagerCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mAction = (TabSelectionEditorBookmarkAction) TabSelectionEditorBookmarkAction.createAction(
                mActivity, ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START);
        mTabModel = spy(new MockTabModel(false, null));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Drawable drawable = AppCompatResources.getDrawable(mActivity, R.drawable.btn_star);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());

        Assert.assertEquals(R.id.tab_selection_editor_bookmark_menu_item,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(R.plurals.tab_selection_editor_bookmark_tabs_action_button,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(true,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_bookmark_tabs_action_button,
                mAction.getPropertyModel()
                        .get(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testBookmarkActionNoTabs() {
        mAction.onSelectionStateChange(new ArrayList<Integer>());
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testBookmarkActionWithOneTab() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(1);

        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }

        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        mAction.setDelegateForTesting(mTabSelectionEditorBookmarkActionDelegate);
        when(mDelegate.getSnackbarManager()).thenReturn(mSnackbarManager);

        Assert.assertTrue(mAction.perform());

        verify(mTabSelectionEditorBookmarkActionDelegate)
                .bookmarkTabsAndShowSnackbar(mActivityCaptor.capture(), mTabsCaptor.capture(),
                        mSnackbarManagerCaptor.capture());

        Activity activityCaptorValue = mActivityCaptor.getValue();
        List<Tab> tabsCaptorValue = mTabsCaptor.getValue();
        SnackbarManager snackbarManagerCaptorValue = mSnackbarManagerCaptor.getValue();

        Assert.assertEquals(mActivity, activityCaptorValue);
        Assert.assertEquals(tabs, tabsCaptorValue);
        Assert.assertEquals(mSnackbarManager, snackbarManagerCaptorValue);

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());

        verify(mTabSelectionEditorBookmarkActionDelegate, times(2))
                .bookmarkTabsAndShowSnackbar(mActivityCaptor.capture(), mTabsCaptor.capture(),
                        mSnackbarManagerCaptor.capture());

        activityCaptorValue = mActivityCaptor.getValue();
        tabsCaptorValue = mTabsCaptor.getValue();
        snackbarManagerCaptorValue = mSnackbarManagerCaptor.getValue();

        Assert.assertEquals(mActivity, activityCaptorValue);
        Assert.assertEquals(tabs, tabsCaptorValue);
        Assert.assertEquals(mSnackbarManager, snackbarManagerCaptorValue);

        Assert.assertEquals(1, helper.getCallCount());
        verify(mDelegate, never()).syncRecyclerViewPosition();
        verify(mDelegate, never()).hideByAction();
    }

    @Test
    @SmallTest
    public void testBookmarkActionWithMultipleTabs() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(1);
        tabIds.add(2);
        tabIds.add(3);

        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        mAction.setDelegateForTesting(mTabSelectionEditorBookmarkActionDelegate);
        when(mDelegate.getSnackbarManager()).thenReturn(mSnackbarManager);

        Assert.assertTrue(mAction.perform());

        verify(mTabSelectionEditorBookmarkActionDelegate)
                .bookmarkTabsAndShowSnackbar(mActivityCaptor.capture(), mTabsCaptor.capture(),
                        mSnackbarManagerCaptor.capture());

        Activity activityCaptorValue = mActivityCaptor.getValue();
        List<Tab> tabsCaptorValue = mTabsCaptor.getValue();
        SnackbarManager snackbarManagerCaptorValue = mSnackbarManagerCaptor.getValue();

        Assert.assertEquals(mActivity, activityCaptorValue);
        Assert.assertEquals(tabs, tabsCaptorValue);
        Assert.assertEquals(mSnackbarManager, snackbarManagerCaptorValue);

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());

        verify(mTabSelectionEditorBookmarkActionDelegate, times(2))
                .bookmarkTabsAndShowSnackbar(mActivityCaptor.capture(), mTabsCaptor.capture(),
                        mSnackbarManagerCaptor.capture());

        activityCaptorValue = mActivityCaptor.getValue();
        tabsCaptorValue = mTabsCaptor.getValue();
        snackbarManagerCaptorValue = mSnackbarManagerCaptor.getValue();

        Assert.assertEquals(mActivity, activityCaptorValue);
        Assert.assertEquals(tabs, tabsCaptorValue);
        Assert.assertEquals(mSnackbarManager, snackbarManagerCaptorValue);

        Assert.assertEquals(1, helper.getCallCount());
        verify(mDelegate, never()).syncRecyclerViewPosition();
        verify(mDelegate, never()).hideByAction();
    }
}
