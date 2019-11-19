// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGridDialogMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
public class TabGridDialogMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String DIALOG_TITLE1 = "1 Tab";
    private static final String DIALOG_TITLE2 = "2 Tabs";
    private static final String REMOVE_BUTTON_STRING = "Remove";
    private static final String CUSTOMIZED_DIALOG_TITLE = "Cool Tabs";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    Rect mRect;
    @Mock
    View mView;
    @Mock
    TabGridDialogMediator.DialogController mDialogController;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreatorManager.TabCreator mTabCreator;
    @Mock
    TabSwitcherMediator.ResetHandler mTabSwitcherResetHandler;
    @Mock
    TabGridDialogMediator.AnimationSourceViewProvider mAnimationSourceViewProvider;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabSelectionEditorCoordinator.TabSelectionEditorController mTabSelectionEditorController;
    @Mock
    TabGroupTitleEditor mTabGroupTitleEditor;
    @Mock
    EditText mTitleTextView;
    @Mock
    Editable mEditable;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private PropertyModel mModel;
    private TabGridDialogMediator mMediator;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mResources).when(mContext).getResources();
        doReturn(DIALOG_TITLE1)
                .when(mResources)
                .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 1, 1);
        doReturn(DIALOG_TITLE2)
                .when(mResources)
                .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2);
        doReturn(mView).when(mAnimationSourceViewProvider).getAnimationSourceViewForTab(anyInt());
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(REMOVE_BUTTON_STRING)
                .when(mContext)
                .getString(R.string.tab_grid_dialog_selection_mode_remove);
        doReturn(mEditable).when(mTitleTextView).getText();
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mEditable).toString();

        if (!FeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            mTabSelectionEditorController = null;
        }
        mModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);
        mMediator =
                new TabGridDialogMediator(mContext, mDialogController, mModel, mTabModelSelector,
                        mTabCreatorManager, mTabSwitcherResetHandler, mAnimationSourceViewProvider,
                        mTabSelectionEditorController, mTabGroupTitleEditor, "");
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void setupListenersAndObservers() {
        // These listeners and observers should be setup when the mediator is created.
        assertThat(mModel.get(TabGridPanelProperties.SCRIMVIEW_OBSERVER),
                instanceOf(ScrimView.ScrimObserver.class));
        assertThat(mModel.get(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
        assertThat(mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void setupTabGroupsContinuation_flagEnabled() {
        assertThat(FeatureUtilities.isTabGroupsAndroidContinuationEnabled(), equalTo(true));
        // Setup editable title.
        assertThat(mMediator.getKeyboardVisibilityListenerForTesting(),
                instanceOf(KeyboardVisibilityDelegate.KeyboardVisibilityListener.class));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER),
                instanceOf(TextWatcher.class));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER),
                instanceOf(View.OnFocusChangeListener.class));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_TOUCH_LISTENER),
                instanceOf(View.OnTouchListener.class));

        // Setup selection editor for ungrouping.
        assertThat(mModel.get(TabGridPanelProperties.MENU_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
        verify(mTabSelectionEditorController)
                .configureToolbar(eq(REMOVE_BUTTON_STRING),
                        any(TabSelectionEditorActionProvider.class), eq(1), eq(null));
    }

    @Test
    public void setupTabGroupsContinuation_flagDisabled() {
        assertThat(FeatureUtilities.isTabGroupsAndroidContinuationEnabled(), equalTo(false));

        assertThat(mMediator.getKeyboardVisibilityListenerForTesting(), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_TOUCH_LISTENER), equalTo(null));

        assertThat(mModel.get(TabGridPanelProperties.MENU_CLICK_LISTENER), equalTo(null));
        assertNull(mTabSelectionEditorController);
    }

    @Test
    public void onClickAdd_HasCurrentTab() {
        // Mock that the animation source view is not null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        View.OnClickListener listener = mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_CHROME_UI), eq(mTab1));
    }

    @Test
    public void onClickAdd_NoCurrentTab() {
        mMediator.setCurrentTabIdForTest(Tab.INVALID_TAB_ID);

        View.OnClickListener listener = mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mTabCreator).launchNTP();
    }

    @Test
    public void onClickCollapse() {
        View.OnClickListener listener = mModel.get(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void onClickScrim() {
        ScrimView.ScrimObserver observer = mModel.get(TabGridPanelProperties.SCRIMVIEW_OBSERVER);
        observer.onScrimClick();

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onTitleTextChange_WithoutFocus() {
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);
        assertThat(mEditable.toString(), equalTo(CUSTOMIZED_DIALOG_TITLE));

        textWatcher.afterTextChanged(mEditable);

        // TabGroupTitleEditor should not react to text change when there is no focus.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), any(String.class));
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(), equalTo(null));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onTitleTextChange_WithFocus() {
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);
        assertThat(mEditable.toString(), equalTo(CUSTOMIZED_DIALOG_TITLE));

        // Focus on title TextView.
        View.OnFocusChangeListener listener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        listener.onFocusChange(mTitleTextView, true);

        textWatcher.afterTextChanged(mEditable);

        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onTitleTextFocusChange() {
        View.OnFocusChangeListener listener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(false));

        listener.onFocusChange(mTitleTextView, true);

        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(true));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onKeyBoardVisibilityChanged_ChangeCursorVisibility() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        mModel.set(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY, false);
        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);

        listener.keyboardVisibilityChanged(true);
        assertThat(mModel.get(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY), equalTo(true));
        assertThat(mModel.get(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED), equalTo(true));

        listener.keyboardVisibilityChanged(false);
        assertThat(mModel.get(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY), equalTo(false));
        assertThat(mModel.get(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onKeyBoardVisibilityChanged_StoreGroupTitle() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        // Mock that keyboard shows and group title is updated.
        keyboardVisibilityListener.keyboardVisibilityChanged(true);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        verify(mTabGroupTitleEditor).storeTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onKeyBoardVisibilityChanged_NoFocus_NotStoreGroupTitle() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        // Mock that keyboard shows but title edit text is not focused.
        keyboardVisibilityListener.keyboardVisibilityChanged(true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(false));

        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onTitleTextTouchEvent() {
        View.OnTouchListener listener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_TOUCH_LISTENER);

        assertThat(mModel.get(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE), equalTo(false));

        listener.onTouch(mTitleTextView, mock(MotionEvent.class));

        assertThat(mModel.get(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE), equalTo(true));
        verify(mTitleTextView).performClick();
    }

    @Test
    public void tabAddition() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source view is not null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void tabClosure_NotLast_NotCurrent() {
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should not update.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosure_NotLast_Current() {
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Mock tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB2_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosure_Last_Current() {
        // Mock that tab1 is the last tab in the group and it just gets closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        // As last tab in the group, tab1 is definitely the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        // Mock the dialog is showing and the animation source view is not null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);

        mMediator.onReset(null);
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(Tab.INVALID_TAB_ID));
    }

    @Test
    public void tabClosure_NotLast_Current_WithDialogHidden() {
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Mock tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB2_ID);
        // Mock dialog title is null and the dialog is hidden.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosure_NonRootTab_StillGroupAfterClosure_WithStoredTitle() {
        // Mock that tab1, tab2 and newTab are in the same group and tab1 is the root tab.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that newTab just get closed.
        List<Tab> tabgroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        assertThat(mTabGroupTitleEditor.getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE));
        mTabModelObserverCaptor.getValue().willCloseTab(newTab, false);

        // Dialog title should still be the stored title.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosure_RootTab_StillGroupAfterClosure_WithStoredTitle() {
        // Mock that tab1, tab2 and newTab are in the same group and newTab is the root tab.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabgroup, TAB3_ID);

        // Mock that newTab just get closed.
        List<Tab> tabgroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of newTab.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB3_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(newTab, false);

        // Dialog title should still be the stored title even if the root tab is closed.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosure_SingleTabAfterClosure_WithStoredTitle() {
        // Mock that tab1, tab2 are in the same group and tab1 is the root tab.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that tab2 just get closed.
        List<Tab> tabgroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1));
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock that tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB2_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Even if there is a stored title for tab1, it is now a single tab, so we won't show the
        // stored title.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
    }

    @Test
    public void tabClosureUndone() {
        // Mock that the dialog is showing.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosureUndone_WithStoredTitle() {
        // Mock that the dialog is showing.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        // Mock that tab1 and tab2 are in the same group, and we are undoing tab2.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        // If current group has a stored title, dialog title should be set to stored title when
        // undoing a closure.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosureUndone_WithDialogHidden() {
        // Mock that the dialog is hidden.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabSelection() {
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void hideDialog_FadeOutAnimation() {
        // Mock that the animation source view is null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_ZoomOutAnimation() {
        // Mock that the animation source view is null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);

        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mMediator.hideDialog(true);

        // Animation source view should be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_StoreModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        verify(mTabGroupTitleEditor).storeTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_ModifiedGroupTitleEmpty() {
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        // Mock that we have a modified group title which is an empty string.
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        doReturn("").when(mEditable).toString();
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(), equalTo(""));

        mMediator.hideDialog(false);

        // When updated title is a empty string, delete stored title and restore default title in
        // PropertyModel.
        verify(mTabGroupTitleEditor).deleteTabGroupTitle(eq(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(DIALOG_TITLE2));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_NoModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        mMediator.hideDialog(false);

        // When title is not updated, don't store title when hide dialog.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_ClosingLastTab_SkipStoreGroupTitle() {
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that the last tab in the group is closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        // Skip storing dialog title when the last tab is closing.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_SingleTab_SkipStoreGroupTitle() {
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is now a single tab.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1)), TAB1_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        // Skip storing dialog title when this group becomes a single tab.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_withTabGroupContinuation() {
        mMediator.hideDialog(false);

        verify(mTabSelectionEditorController).hide();
    }

    @Test
    public void hideDialog_onReset() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.onReset(null);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
    }

    @Test
    public void showDialog_FromGTS() {
        // Mock that the dialog is hidden and animation source view and header title are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
    }

    @Test
    public void showDialog_FromGTS_WithStoredTitle() {
        // Mock that the dialog is hidden and animation source view and header title are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated with stored title.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void showDialog_FromStrip() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        mMediator = new TabGridDialogMediator(mContext, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler, null,
                mTabSelectionEditorController, mTabGroupTitleEditor, "");

        // Mock that the dialog is hidden and animation source view and header title are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
    }

    @Test
    public void showDialog_FromStrip_WithStoredTitle() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        mMediator = new TabGridDialogMediator(mContext, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler, null,
                mTabSelectionEditorController, mTabGroupTitleEditor, "");
        // Mock that the dialog is hidden and animation source view and header title are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated with stored title.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testDialogToolbarMenu_SelectionMode() {
        Callback<Integer> callback = mMediator.getToolbarMenuCallbackForTesting();
        // Mock that currently the popup window is focusable, and the current tab is tab1 which is
        // in a group of {tab1, tab2}.
        mModel.set(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE, true);
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        callback.onResult(R.id.ungroup_tab);

        assertFalse(mModel.get(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE));
        verify(mTabSelectionEditorController).show(eq(tabgroup));
    }

    @Test
    public void destroy() {
        mMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverCaptor.capture());
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(id).when(tab).getRootId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(true).when(tab).isIncognito();
        return tab;
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            doReturn(rootId).when(tab).getRootId();
        }
    }
}