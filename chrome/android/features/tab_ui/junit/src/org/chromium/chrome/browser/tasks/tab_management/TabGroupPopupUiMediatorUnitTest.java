// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

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

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupPopupUiMediator}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupPopupUiMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;

    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock
    TabGroupPopupUiMediator.TabGroupPopUiUpdater mUpdater;
    @Mock
    TabGroupUiMediator.TabGroupUiController mTabGroupUiController;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock
    ToolbarPhone mTopAnchorView;
    @Mock
    FrameLayout mBottomAnchorView;
    @Mock
    BottomSheetController mBottomSheetController;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderObserverCaptor;
    @Captor
    ArgumentCaptor<OverviewModeBehavior.OverviewModeObserver> mOverviewModeObserverCaptor;
    @Captor
    ArgumentCaptor<KeyboardVisibilityDelegate.KeyboardVisibilityListener>
            mKeyboardVisibilityListenerCaptor;
    @Captor
    ArgumentCaptor<BottomSheetObserver> mBottomSheetObserver;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private PropertyModel mModel;
    private TabGroupPopupUiMediator mMediator;
    private OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = prepareTab(TAB3_ID, TAB3_TITLE);

        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderObserverCaptor.capture());
        doNothing()
                .when(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        doNothing()
                .when(mKeyboardVisibilityDelegate)
                .addKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserver.capture());

        mOverviewModeBehaviorSupplier.set(mOverviewModeBehavior);
        KeyboardVisibilityDelegate.setInstance(mKeyboardVisibilityDelegate);
        mModel = new PropertyModel(TabGroupPopupUiProperties.ALL_KEYS);
        mMediator = new TabGroupPopupUiMediator(mModel, mTabModelSelector,
                mOverviewModeBehaviorSupplier, mBrowserControlsStateProvider, mUpdater,
                mTabGroupUiController, mBottomSheetController);
    }

    @Test
    public void testOnControlOffsetChanged() {
        mModel.set(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA, 0f);

        // Mock that the hidden ratio of browser control is 0.8765.
        float hiddenRatio = 0.8765f;
        doReturn(hiddenRatio).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();
        mBrowserControlsStateProviderObserverCaptor.getValue().onControlsOffsetChanged(
                0, 0, 0, 0, false);

        assertThat(
                mModel.get(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA), equalTo(1 - hiddenRatio));

        // Mock that the hidden ratio of browser control is 0.12345.
        hiddenRatio = 0.1234f;
        doReturn(hiddenRatio).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();
        mBrowserControlsStateProviderObserverCaptor.getValue().onControlsOffsetChanged(
                0, 0, 0, 0, false);

        assertThat(
                mModel.get(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA), equalTo(1 - hiddenRatio));
    }

    @Test
    public void tabSelection_Show() {
        // Mock that the strip is hidden.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 and tab2 are in the same group, and tab 3 is a single tab.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        createTabGroup(new ArrayList<>(Arrays.asList(mTab3)), TAB3_ID);

        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB3_ID);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabSelection_Hide() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1 and tab2 are in the same group, and tab 3 is a single tab.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        createTabGroup(new ArrayList<>(Arrays.asList(mTab3)), TAB3_ID);

        doReturn(mTab3).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabSelection_Update() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1 and tab2 are in the same group, tab3 and new tab are in the same group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        createTabGroup(
                new ArrayList<>(Arrays.asList(mTab3, prepareTab(TAB4_ID, TAB4_TITLE))), TAB3_ID);

        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TAB3_ID);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater).updateTabGroupPopUi();
    }

    @Test
    public void tabSelection_SameGroup() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1 and tab2 are in the same group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);

        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TAB2_ID);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabClosure_Hide() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1 and tab2 are in the same group, and tab1 is closing.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab2));
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabClosure_Update() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1, tab2 and tab3 are in the same group, and tab1 is closing.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3));
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater).updateTabGroupPopUi();
    }

    @Test
    public void tabAddition_Show() {
        // Mock that the strip is hidden.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 and tab2 are in the same group, and tab2 has just been created.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().didAddTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabAddition_Update() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1, tab2 and tab3 are in the same group, and tab3 has just been created.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().didAddTab(
                mTab3, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater).updateTabGroupPopUi();
    }

    @Test
    public void tabAddition_NotShow_Restore() {
        // Mock that the strip is hidden.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 and tab2 are in the same group, and they are being restored.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().didAddTab(
                mTab2, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabClosureUndone_Show() {
        // Mock that the strip is hiding.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 and tab2 are in the same group, and we have just undone the closure of
        // tab2.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabClosureUndone_Update() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1, tab2 and tab3 are in the same group, and we have just undone the closure
        // of tab3.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab3);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
        verify(mUpdater).updateTabGroupPopUi();
    }

    @Test
    public void tabClosureUndone_NotShow() {
        // Mock that the strip is hiding.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 is a single tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabGroup, TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        verify(mUpdater, never()).updateTabGroupPopUi();
    }

    @Test
    public void tabRestoreCompletion_NotShow() {
        // Mock that the strip is hiding.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 is the current tab and it is a single tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mTabModelObserverCaptor.getValue().restoreCompleted();

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void tabRestoreCompletion_Show() {
        // Mock that the strip is hiding.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 is the current tab and it is in a group of {tab1, tab2}.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mTabModelObserverCaptor.getValue().restoreCompleted();

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void testOverviewModeHiding() {
        // Mock that the strip is hiding.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 and tab2 are in the same group, and tab1 is the current tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();

        assertThat(mMediator.getIsOverviewModeVisibleForTesting(), equalTo(false));
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void testOverviewModeShowing() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        // Mock that tab1 and tab2 are in the same group, and tab1 is the current tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(true);

        assertThat(mMediator.getIsOverviewModeVisibleForTesting(), equalTo(true));
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testNeverShowStripWhenOverviewVisible() {
        // Mock that the strip is hiding and overview is visible.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(true);
        assertThat(mMediator.getIsOverviewModeVisibleForTesting(), equalTo(true));

        // Calling maybeShowTabStrip should never show strip in this case.
        mMediator.maybeShowTabStrip();
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testNeverShowStripWhenSingleTab() {
        // Mock that the strip is hiding and overview is visible.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        // Mock that tab1 is the current tab and it is a single tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        // Calling maybeShowTabStrip should never show strip in this case.
        mMediator.maybeShowTabStrip();
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testShowKeyboard_HideStrip() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);

        mKeyboardVisibilityListenerCaptor.getValue().keyboardVisibilityChanged(true);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testHideKeyboard_ShowStrip() {
        // Mock that the strip is showing before showing the keyboard. tab1 and tab2 are in the same
        // group, and tab1 is the current tab.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        // Hide the keyboard after showing it.
        mKeyboardVisibilityListenerCaptor.getValue().keyboardVisibilityChanged(true);
        mKeyboardVisibilityListenerCaptor.getValue().keyboardVisibilityChanged(false);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void testHideKeyboard_NotReshowStrip() {
        // Mock that the strip is hidden before showing the keyboard. tab1 and tab2 are in the same
        // group, and tab1 is the current tab.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        // Hide the keyboard after showing it.
        mKeyboardVisibilityListenerCaptor.getValue().keyboardVisibilityChanged(true);
        mKeyboardVisibilityListenerCaptor.getValue().keyboardVisibilityChanged(false);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testShowBottomSheet_HideStrip() {
        // Mock that the strip is showing.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);

        // Show bottom sheet.
        mBottomSheetObserver.getValue().onSheetStateChanged(BottomSheetController.SheetState.PEEK);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testHideBottomSheet_ShowStrip() {
        // Mock that the strip is showing before showing the bottom sheet. tab1 and tab2 are in the
        // same group, and tab1 is the current tab.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        // Hide the bottom sheet after showing it.
        mBottomSheetObserver.getValue().onSheetStateChanged(BottomSheetController.SheetState.PEEK);
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        mBottomSheetObserver.getValue().onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(true));
    }

    @Test
    public void testHideBottomSheet_NotReshowStrip() {
        // Mock that the strip is hidden before showing the bottom sheet. tab1 and tab2 are in the
        // same group, and tab1 is the current tab.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        // Hide the bottom sheet after showing it.
        mBottomSheetObserver.getValue().onSheetStateChanged(BottomSheetController.SheetState.PEEK);
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
        mBottomSheetObserver.getValue().onSheetStateChanged(
                BottomSheetController.SheetState.HIDDEN);

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testAnchorViewChange_TopToolbar() {
        mMediator.onAnchorViewChanged(mTopAnchorView, R.id.toolbar);

        assertThat(mModel.get(TabGroupPopupUiProperties.ANCHOR_VIEW), equalTo(mTopAnchorView));
        verify(mTabGroupUiController)
                .setupLeftButtonDrawable(eq(R.drawable.ic_expand_less_black_24dp));
    }

    @Test
    public void testAnchorViewChange_BottomToolbar() {
        mMediator.onAnchorViewChanged(mBottomAnchorView, R.id.bottom_controls);

        assertThat(mModel.get(TabGroupPopupUiProperties.ANCHOR_VIEW), equalTo(mBottomAnchorView));
        verify(mTabGroupUiController)
                .setupLeftButtonDrawable(eq(R.drawable.ic_expand_more_black_24dp));
    }

    @Test
    public void testAnchorViewChange_WithStripShowing() {
        // Mock that strip is showing when anchor view changes.
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);

        mMediator.onAnchorViewChanged(mBottomAnchorView, R.id.bottom_controls);

        assertTrue(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE));
        assertThat(mModel.get(TabGroupPopupUiProperties.ANCHOR_VIEW), equalTo(mBottomAnchorView));
        verify(mTabGroupUiController)
                .setupLeftButtonDrawable(eq(R.drawable.ic_expand_more_black_24dp));
    }

    @Test
    public void testNoCurrentTab_NotShow() {
        // Mock overview mode is hiding, and current tab is null.
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();
        assertThat(mMediator.getIsOverviewModeVisibleForTesting(), equalTo(false));
        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));

        mMediator.maybeShowTabStrip();

        assertThat(mModel.get(TabGroupPopupUiProperties.IS_VISIBLE), equalTo(false));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mKeyboardVisibilityDelegate)
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListenerCaptor.capture());
        verify(mOverviewModeBehavior)
                .removeOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mBrowserControlsStateProvider)
                .removeObserver(mBrowserControlsStateProviderObserverCaptor.capture());
    }

    // TODO(yuezhanggg): Pull methods below to a utility class.
    private TabImpl prepareTab(int id, String title) {
        TabImpl tab = TabUiUnitTestUtils.prepareTab(id, title, "");
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
