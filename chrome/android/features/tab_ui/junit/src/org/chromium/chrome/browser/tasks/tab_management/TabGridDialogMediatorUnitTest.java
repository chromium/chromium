// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertNotNull;
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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView.RecyclerViewPosition;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGridDialogMediator}.
 */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// clang-format off
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
@Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
public class TabGridDialogMediatorUnitTest {
    // clang-format on

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String DIALOG_TITLE1 = "1 tab";
    private static final String DIALOG_TITLE2 = "2 tabs";
    private static final String REMOVE_BUTTON_STRING = "Remove";
    private static final String CUSTOMIZED_DIALOG_TITLE = "Cool Tabs";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    View mView;
    @Mock
    TabGridDialogMediator.DialogController mDialogController;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreator mTabCreator;
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
    @Mock
    SnackbarManager mSnackbarManager;
    @Mock
    Supplier<RecyclerViewPosition> mRecyclerViewPositionSupplier;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private Activity mActivity;
    private PropertyModel mModel;
    private TabGridDialogMediator mMediator;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

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
        doReturn(mView).when(mAnimationSourceViewProvider).getAnimationSourceViewForTab(anyInt());
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(mEditable).when(mTitleTextView).getText();
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mEditable).toString();
        doReturn(null).when(mRecyclerViewPositionSupplier).get();

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);
        mMediator = new TabGridDialogMediator(mActivity, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler,
                mRecyclerViewPositionSupplier, mAnimationSourceViewProvider, mSnackbarManager, "");

        // TabModelObserver is registered when native is ready.
        assertThat(mTabModelObserverCaptor.getAllValues().isEmpty(), equalTo(true));
        mMediator.initWithNative(
                () -> { return mTabSelectionEditorController; }, mTabGroupTitleEditor);
        assertThat(mTabModelObserverCaptor.getAllValues().isEmpty(), equalTo(false));
    }

    @Test
    public void setupListenersAndObservers() {
        // These listeners and observers should be setup when the mediator is created.
        assertThat(mModel.get(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
        assertThat(mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void setupTabGroupsContinuation_flagEnabled() {
        assertThat(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                           ContextUtils.getApplicationContext()),
                equalTo(true));
        // Setup editable title.
        assertThat(mMediator.getKeyboardVisibilityListenerForTesting(),
                instanceOf(KeyboardVisibilityDelegate.KeyboardVisibilityListener.class));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER),
                instanceOf(TextWatcher.class));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER),
                instanceOf(View.OnFocusChangeListener.class));
    }

    @Test
    public void setupTabSelectionEditorV2() {
        assertThat(mMediator.getKeyboardVisibilityListenerForTesting(), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER), equalTo(null));

        // Setup selection editor for multiple items.
        assertThat(mModel.get(TabGridPanelProperties.MENU_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));

        ArgumentCaptor<List<TabSelectionEditorAction>> captor =
                ArgumentCaptor.forClass((Class) List.class);
        mMediator.getToolbarMenuCallbackForTesting().onResult(R.id.select_tabs);
        verify(mTabSelectionEditorController)
                .configureToolbarWithMenuItems(captor.capture(), eq(null));
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabSelectionEditorController).show(any(), eq(0), eq(null));
        List<TabSelectionEditorAction> actions = captor.getValue();
        assertThat(actions.get(0), instanceOf(TabSelectionEditorSelectionAction.class));
        assertThat(actions.get(1), instanceOf(TabSelectionEditorCloseAction.class));
        assertThat(actions.get(2), instanceOf(TabSelectionEditorUngroupAction.class));
    }

    @Test
    public void setupTabGroupsContinuation_flagDisabled() {
        assertThat(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                           ContextUtils.getApplicationContext()),
                equalTo(false));

        assertThat(mMediator.getKeyboardVisibilityListenerForTesting(), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER), equalTo(null));

        assertNotNull(mModel.get(TabGridPanelProperties.MENU_CLICK_LISTENER));
        assertNotNull(mTabSelectionEditorController);
    }

    @Test
    public void onClickAdd_HasCurrentTab() {
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        View.OnClickListener listener = mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTab1));
    }

    @Test
    public void onClickAdd_NoCurrentTab() {
        mMediator.setCurrentTabIdForTesting(Tab.INVALID_TAB_ID);

        View.OnClickListener listener = mModel.get(TabGridPanelProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mTabCreator).launchNTP();
    }

    @Test
    public void onClickCollapse() {
        // Show the group of {tab1, tab2} in dialog to trigger the set of scrim observer.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);
        mMediator.onReset(tabgroup);

        View.OnClickListener listener = mModel.get(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void onClickScrim() {
        // Show the group of {tab1, tab2} in dialog to trigger the set of scrim observer.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);
        mMediator.onReset(tabgroup);

        Runnable scrimClickRunnable = mModel.get(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE);
        scrimClickRunnable.run();

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onTitleTextChange_WithoutFocus() {
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
    public void onKeyBoardVisibilityChanged_updateTextAndKeyboard() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        mModel.set(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY, false);
        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);

        listener.keyboardVisibilityChanged(true);
        assertThat(mModel.get(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY), equalTo(true));
        assertThat(mModel.get(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        assertThat(mModel.get(TabGridPanelProperties.IS_KEYBOARD_VISIBLE), equalTo(false));

        listener.keyboardVisibilityChanged(false);
        assertThat(mModel.get(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY), equalTo(false));
        assertThat(mModel.get(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        assertThat(mModel.get(TabGridPanelProperties.IS_KEYBOARD_VISIBLE), equalTo(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void onKeyBoardVisibilityChanged_StoreGroupTitle() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridPanelProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
    public void tabAddition() {
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);

        // Current tab ID should not update.
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(TAB1_ID));
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
        mMediator.setCurrentTabIdForTesting(TAB2_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosure_Last_Current() {
        // Mock that tab1 is the last tab in the group and it just gets closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        // As last tab in the group, tab1 is definitely the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        // Mock the dialog is showing and the animation source view is not null.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false, true);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);

        mMediator.onReset(null);
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(Tab.INVALID_TAB_ID));
    }

    @Test
    public void tabClosure_NotLast_Current_WithDialogHidden() {
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Mock tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB2_ID);
        // Mock dialog title is null and the dialog is hidden.
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);
    }

    @Test
    public void tabClosure_NonRootTab_StillGroupAfterClosure_WithStoredTitle() {
        // Mock that tab1, tab2 and newTab are in the same group and tab1 is the root tab.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that newTab just get closed.
        List<Tab> tabgroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        assertThat(mTabGroupTitleEditor.getTabGroupTitle(
                           CriticalPersistedTabData.from(mTab1).getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE));
        mTabModelObserverCaptor.getValue().willCloseTab(newTab, false, true);

        // Dialog title should still be the stored title.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosure_RootTab_StillGroupAfterClosure_WithStoredTitle() {
        // Mock that tab1, tab2 and newTab are in the same group and newTab is the root tab.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabgroup, TAB3_ID);

        // Mock that newTab just get closed.
        List<Tab> tabgroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabgroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of newTab.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB3_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(newTab, false, true);

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
        mMediator.setCurrentTabIdForTesting(TAB2_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);

        // Even if there is a stored title for tab1, it is now a single tab, so we won't show the
        // stored title.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
    }

    @Test
    public void tabClosureUndone() {
        // Mock that the dialog is showing.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false, false);
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void tabClosureUndone_WithStoredTitle() {
        // Mock that the dialog is showing.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

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
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB2_ID));
    }

    @Test
    public void tabClosureUndone_WithDialogHidden() {
        // Mock that the dialog is hidden.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never())
                .resetWithTabList(mTabGroupModelFilter, false, false);
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void tabClosureCommitted() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void onFinishingMultipleTabClosure() {
        List<Tab> tabs = Arrays.asList(mTab1, mTab2);
        mTabModelObserverCaptor.getValue().onFinishingMultipleTabClosure(tabs);

        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(tabs));
    }

    @Test
    public void onFinishingMultipleTabClosure_singleTab() {
        List<Tab> tabs = Arrays.asList(mTab1);
        mTabModelObserverCaptor.getValue().onFinishingMultipleTabClosure(tabs);

        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void allTabsClosureCommitted() {
        mTabModelObserverCaptor.getValue().allTabsClosureCommitted(false);

        verify(mSnackbarManager).dismissSnackbars(eq(mMediator));
    }

    @Test
    public void tabPendingClosure_DialogVisible() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().tabPendingClosure(mTab1);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void tabPendingClosure_DialogInvisible() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().tabPendingClosure(mTab1);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_DialogVisible() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().multipleTabsPendingClosure(
                Arrays.asList(mTab1, mTab2), false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_singleTab_DialogVisible() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().multipleTabsPendingClosure(Arrays.asList(mTab1), false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_DialogInvisible() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().multipleTabsPendingClosure(
                Arrays.asList(mTab1, mTab2), false);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void tabSelection() {
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void hideDialog_FadeOutAnimation() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_WithVisibilityListener_BasicAnimation() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        // Set visibility listener.
        mModel.set(TabGridPanelProperties.VISIBILITY_LISTENER, mMediator);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verifyNoMoreInteractions(mDialogController);
    }

    @Test
    public void hideDialog_FadeOutAnimation_ClearsViewAnimation() {
        // Mock that the animation source view is set, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_WithVisibilityListener_ClearsViewAnimation() {
        // Mock that the animation source view exists, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        // Set visibility listener.
        mModel.set(TabGridPanelProperties.VISIBILITY_LISTENER, mMediator);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verifyNoMoreInteractions(mDialogController);
    }

    @Test
    public void hideDialog_ZoomOutAnimation() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mMediator.hideDialog(true);

        // Animation source view should be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void hideDialog_StoreModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, TAB1_TITLE);
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
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
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        verify(mTabSelectionEditorController).hide();
    }

    @Test
    public void onReset_hideDialog() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);

        mMediator.onReset(null);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mDialogController).postHiding();
    }

    @Test
    public void onReset_DialogNotVisible_NoOp() {
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);

        mMediator.onReset(null);

        verifyNoMoreInteractions(mDialogController);
    }

    @Test
    public void finishedHiding() {
        mMediator.finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(null);
        verify(mDialogController).postHiding();
    }

    @Test
    public void showDialog_FromGTS() {
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim click runnable should be set as the current scrim runnable.
        assertThat(mModel.get(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        // Prepare dialog invoked.
        verify(mDialogController).prepareDialog();
    }

    @Test
    public void showDialog_FromGTS_WithStoredTitle() {
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim click runnable should be set as the current scrim runnable.
        assertThat(mModel.get(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
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
        mMediator = new TabGridDialogMediator(mActivity, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler,
                mRecyclerViewPositionSupplier, null, mSnackbarManager, "");
        mMediator.initWithNative(
                () -> { return mTabSelectionEditorController; }, mTabGroupTitleEditor);

        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim observer should be set as the current scrim runnable.
        assertThat(mModel.get(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        // Prepare dialog invoked.
        verify(mDialogController).prepareDialog();
    }

    @Test
    public void showDialog_FromStrip_WithStoredTitle() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        mMediator = new TabGridDialogMediator(mActivity, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler,
                mRecyclerViewPositionSupplier, null, mSnackbarManager, "");
        mMediator.initWithNative(
                () -> { return mTabSelectionEditorController; }, mTabGroupTitleEditor);
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridPanelProperties.HEADER_TITLE, null);
        mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim observer should be set as the current scrim click runnable.
        assertThat(mModel.get(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated with stored title.
        assertThat(
                mModel.get(TabGridPanelProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void showDialog_FromStrip_SetupAnimation() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        mMediator = new TabGridDialogMediator(mActivity, mDialogController, mModel,
                mTabModelSelector, mTabCreatorManager, mTabSwitcherResetHandler,
                mRecyclerViewPositionSupplier, null, mSnackbarManager, "");
        mMediator.initWithNative(
                () -> { return mTabSelectionEditorController; }, mTabGroupTitleEditor);
        // Mock that the dialog is hidden and animation source view is set to some mock view for
        // testing purpose.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, mock(View.class));
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        mMediator.onReset(tabgroup);

        assertThat(mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should be set to null so that dialog will setup basic animation.
        assertThat(mModel.get(TabGridPanelProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testDialogToolbarMenu_SelectionModeV2() {
        Callback<Integer> callback = mMediator.getToolbarMenuCallbackForTesting();
        // Mock that currently the title text is focused and the keyboard is showing. The current
        // tab is tab1 which is in a group of {tab1, tab2}.
        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, true);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabgroup, TAB1_ID);

        callback.onResult(R.id.select_tabs);

        assertThat(mModel.get(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabSelectionEditorController).show(eq(tabgroup), eq(0), eq(null));
    }

    @Test
    public void testSnackbarController_onAction_singleTab() {
        doReturn(mTabModel).when(mTabModelSelector).getModelForTabId(TAB1_ID);

        mMediator.onAction(TAB1_ID);

        verify(mTabModel).cancelTabClosure(eq(TAB1_ID));
    }

    @Test
    public void testSnackbarController_onAction_multipleTabs() {
        doReturn(mTabModel).when(mTabModelSelector).getModelForTabId(TAB1_ID);

        mMediator.onAction(Arrays.asList(mTab1, mTab2));

        verify(mTabModel).cancelTabClosure(eq(TAB1_ID));
        verify(mTabModel).cancelTabClosure(eq(TAB2_ID));
    }

    @Test
    public void testSnackbarController_onDismissNoAction_singleTab() {
        doReturn(mTabModel).when(mTabModelSelector).getModelForTabId(TAB1_ID);

        mMediator.onDismissNoAction(TAB1_ID);

        verify(mTabModel).commitTabClosure(eq(TAB1_ID));
    }

    @Test
    public void testSnackbarController_onDismissNoAction_multipleTabs() {
        doReturn(mTabModel).when(mTabModelSelector).getModelForTabId(TAB1_ID);

        mMediator.onDismissNoAction(Arrays.asList(mTab1, mTab2));

        verify(mTabModel).commitTabClosure(eq(TAB1_ID));
        verify(mTabModel).commitTabClosure(eq(TAB2_ID));
    }

    @Test
    public void testScrollToTab() {
        // Mock that tab1, tab2 and newTab are in the same group and newTab is the root tab.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabgroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabgroup, TAB2_ID);

        // Mock that mTab2 is the current tab for the dialog.
        doReturn(0).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(TAB2_ID).when(mTabModelSelector).getCurrentTabId();
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(tabgroup).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Reset and confirm scroll index.
        mMediator.onReset(tabgroup);

        Assert.assertEquals(1, mModel.get(TabGridPanelProperties.INITIAL_SCROLL_INDEX).intValue());
    }

    @Test
    public void destroy() {
        mMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverCaptor.capture());
    }

    private TabImpl prepareTab(int id, String title) {
        TabImpl tab = TabUiUnitTestUtils.prepareTab(id, title, GURL.emptyGURL());
        doReturn(true).when(tab).isIncognito();
        return tab;
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            CriticalPersistedTabData criticalPersistedTabData = CriticalPersistedTabData.from(tab);
            doReturn(rootId).when(criticalPersistedTabData).getRootId();
        }
    }
}
