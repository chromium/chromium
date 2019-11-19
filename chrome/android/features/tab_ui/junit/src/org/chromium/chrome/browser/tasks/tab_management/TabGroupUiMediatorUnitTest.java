// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupUiMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUiMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;

    @Mock
    BottomControlsCoordinator.BottomControlsVisibilityController mVisibilityController;
    @Mock
    TabGroupUiMediator.ResetHandler mResetHandler;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreatorManager.TabCreator mTabCreator;
    @Mock
    OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    ThemeColorProvider mThemeColorProvider;
    @Mock
    TabModel mTabModel;
    @Mock
    View mView;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabGridDialogMediator.DialogController mTabGridDialogController;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<OverviewModeBehavior.OverviewModeObserver> mOverviewModeObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> mThemeColorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.TintObserver> mTintObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverArgumentCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private List<Tab> mTabGroup1;
    private List<Tab> mTabGroup2;
    private PropertyModel mModel;
    private TabGroupUiMediator mTabGroupUiMediator;
    private InOrder mResetHandlerInOrder;
    private InOrder mVisibilityControllerInOrder;

    private Tab prepareTab(int tabId, int rootId) {
        Tab tab = mock(Tab.class);
        doReturn(tabId).when(tab).getId();
        doReturn(rootId).when(tab).getRootId();
        return tab;
    }

    private TabModel prepareIncognitoTabModel() {
        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);
        TabModel incognitoTabModel = mock(TabModel.class);
        doReturn(newTab).when(incognitoTabModel).getTabAt(POSITION1);
        doReturn(true).when(incognitoTabModel).isIncognito();
        doReturn(1).when(incognitoTabModel).getCount();
        return incognitoTabModel;
    }

    private void verifyNeverReset() {
        mResetHandlerInOrder.verify(mResetHandler, never()).resetStripWithListOfTabs(any());
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    private void verifyResetStrip(boolean isVisible, @Nullable List<Tab> tabs) {
        mResetHandlerInOrder.verify(mResetHandler).resetStripWithListOfTabs(tabs);
        mVisibilityControllerInOrder.verify(mVisibilityController)
                .setBottomControlsVisible(isVisible);
    }

    private void initAndAssertProperties(@Nullable Tab currentTab) {
        if (currentTab == null) {
            doReturn(TabModel.INVALID_TAB_INDEX).when(mTabModel).index();
            doReturn(0).when(mTabModel).getCount();
            doReturn(0).when(mTabGroupModelFilter).getCount();
            doReturn(null).when(mTabModelSelector).getCurrentTab();
        } else {
            doReturn(mTabModel.indexOf(currentTab)).when(mTabModel).index();
            doReturn(currentTab).when(mTabModelSelector).getCurrentTab();
        }

        mTabGroupUiMediator = new TabGroupUiMediator(mVisibilityController, mResetHandler, mModel,
                mTabModelSelector, mTabCreatorManager, mOverviewModeBehavior, mThemeColorProvider,
                mTabGridDialogController);

        if (currentTab == null) {
            verifyNeverReset();
            return;
        }

        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(currentTab.getId());

        if (tabs.size() < 2) {
            verifyResetStrip(false, null);
        } else {
            verifyResetStrip(true, tabs);
        }
    }

    @Before
    public void setUp() {
        // Each test must call initAndAssertProperties. After setUp() and
        // initAndAssertProperties(true), TabModel has 3 tabs in the following order: mTab1, mTab2,
        // and mTab3, while mTab2 and mTab3 are in a group. By default mTab1 is selected. If
        // initAndAssertProperties(false) is called instead, there's no tabs in TabModel.
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        // Set up Tabs
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID);
        mTabGroup1 = new ArrayList<>(Arrays.asList(mTab1));
        mTabGroup2 = new ArrayList<>(Arrays.asList(mTab2, mTab3));

        // Set up TabModel.
        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(3).when(mTabModel).getCount();
        doReturn(0).when(mTabModel).index();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab3).when(mTabModel).getTabAt(2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);

        // Set up TabGroupModelFilter.
        doReturn(false).when(mTabGroupModelFilter).isIncognito();
        doReturn(2).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab3);
        doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        // Set up TabModelSelector, TabModelFilterProvider, TabGroupModelFilter.
        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doNothing()
                .when(mTabModelSelector)
                .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        doReturn(tabModelList).when(mTabModelSelector).getModels();

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());

        // Set up OverviewModeBehavior
        doNothing()
                .when(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());

        // Set up ThemeColorProvider
        doNothing()
                .when(mThemeColorProvider)
                .addThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        doNothing()
                .when(mThemeColorProvider)
                .addTintObserver(mTintObserverArgumentCaptor.capture());

        // Set up ResetHandler
        doNothing().when(mResetHandler).resetStripWithListOfTabs(any());
        doNothing().when(mResetHandler).resetGridWithListOfTabs(any());

        // Set up TabCreatorManager
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(null).when(mTabCreator).createNewTab(any(), anyInt(), any());

        mResetHandlerInOrder = inOrder(mResetHandler);
        mVisibilityControllerInOrder = inOrder(mVisibilityController);
        mModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void verifyInitialization_NoTab() {
        initAndAssertProperties(null);
    }

    @Test
    public void verifyInitialization_SingleTab() {
        initAndAssertProperties(mTab1);
    }

    @Test
    public void verifyInitialization_TabGroup() {
        // Tab 2 is in a tab group.
        initAndAssertProperties(mTab2);
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void onClickExpand() {
        // clang-format on
        initAndAssertProperties(mTab2);

        View.OnClickListener listener =
                mModel.get(TabStripToolbarViewProperties.EXPAND_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mResetHandler).resetGridWithListOfTabs(mTabGroup2);
    }

    @Test
    public void onClickAdd() {
        initAndAssertProperties(mTab1);

        View.OnClickListener listener =
                mModel.get(TabStripToolbarViewProperties.ADD_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_CHROME_UI), eq(mTab1));
    }

    @Test
    public void tabSelection_NotSameGroup_SingleTab() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be showing since tab 1 is a single tab.
        verifyResetStrip(false, null);
    }

    @Test
    public void tabSelection_NotSameGroup_TabGroup() {
        initAndAssertProperties(mTab2);

        // Mock that tab 1 is not a single tab.
        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should be showing since we are selecting a group.
        verifyResetStrip(true, tabs);
    }

    @Test
    public void tabSelection_SameGroup_TabGroup() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 3, and the last selected tab is tab 2 which is in the same group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be reset since we are selecting in one group.
        verifyNeverReset();
    }

    @Test
    public void tabClosure_NotLastTabInGroup() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2, and tab 3 then gets selected. They are in the same group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);

        // Strip should not be reset since we are still in this group.
        verifyNeverReset();
    }

    @Test
    public void tabClosure_LastTabInGroup_GroupUiNotVisible() {
        initAndAssertProperties(mTab1);

        // Mock closing tab 1, and tab 2 then gets selected. They are in different group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab1, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_CLOSE, TAB1_ID);

        // Strip should never be reset since currently tab group UI is invisible.
        verifyNeverReset();
    }

    // TODO(988199): Ignore this test until we have a conclusion from the attached bug.
    @Ignore
    @Test
    public void tabClosure_LastTabInGroup_GroupUiVisible() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2 and tab, then tab 1 gets selected. They are in different group. Right
        // now tab group UI is visible.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab3, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_CLOSE, TAB3_ID);

        // Strip should be reset since tab group UI was visible and now we are switching to a
        // different group.
        verifyResetStrip(false, null);
    }

    @Test
    public void tabAddition_SingleTab() {
        initAndAssertProperties(mTab1);

        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_RESTORE);

        // Strip should be not be reset when adding a single new tab.
        verifyNeverReset();
    }

    @Test
    public void tabAddition_TabGroup_NoRefresh() {
        initAndAssertProperties(mTab2);

        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_RESTORE);

        // Strip should be not be reset through these two types of launching.
        verifyNeverReset();
    }

    @Test
    public void tabAddition_TabGroup_Refresh() {
        initAndAssertProperties(mTab2);

        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_LONGPRESS_BACKGROUND);

        // Strip should be be reset when long pressing a link and add a tab into group.
        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    public void restoreCompleted_TabModelNoTab() {
        // Simulate no tab in current TabModel.
        initAndAssertProperties(null);

        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    public void restoreCompleted_UiNotVisible() {
        // Assume mTab1 is selected, and it does not have related tabs.
        initAndAssertProperties(mTab1);
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);
    }

    @Test
    public void restoreCompleted_UiVisible() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(mTab2);
        doReturn(POSITION2).when(mTabModel).index();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(true);
    }

    @Test
    public void tabClosureUndone_UiVisible_NotShowingOverviewMode() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(mTab2);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that another member of this group, newTab, is being undone from closure.
        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab2, mTab3, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Since the strip is already visible, no resetting.
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    public void tabClosureUndone_UiNotVisible_NotShowingOverviewMode() {
        // Assume mTab1 is selected. Since mTab1 is now a single tab, the strip is invisible.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that newTab which was a tab in the same group as mTab1 is being undone from
        // closure.
        Tab newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Strip should reset to be visible.
        mVisibilityControllerInOrder.verify(mVisibilityController)
                .setBottomControlsVisible(eq(true));
    }

    @Test
    public void tabClosureUndone_UiNotVisible_ShowingOverviewMode() {
        // Assume mTab1 is selected.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate the overview mode is showing, which hides the strip.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(true));
        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);

        // Simulate that we undo a group closure of {mTab2, mTab3}.
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab2);

        // Since overview mode is showing, we should not show strip.
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    public void overViewStartedShowing() {
        initAndAssertProperties(mTab1);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);

        verifyResetStrip(false, null);
    }

    @Test
    public void overViewFinishedHiding_NoCurrentTab() {
        initAndAssertProperties(null);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyNeverReset();
    }

    @Test
    public void overViewFinishedHiding_CurrentTabSingle() {
        initAndAssertProperties(mTab1);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyResetStrip(false, null);
    }

    @Test
    public void overViewFinishedHiding_CurrentTabInGroup() {
        initAndAssertProperties(mTab2);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    public void switchToIncognitoTabModel() {
        initAndAssertProperties(mTab1);

        TabModel incognitoTabModel = prepareIncognitoTabModel();

        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                incognitoTabModel, mTabModel);

        verifyResetStrip(false, null);
    }

    @Test
    public void switchToNormalTabModel() {
        initAndAssertProperties(mTab1);

        TabModel incognitoTabModel = prepareIncognitoTabModel();

        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verifyNeverReset();
    }

    @Test
    public void themeColorChange() {
        initAndAssertProperties(mTab1);
        mModel.set(TabStripToolbarViewProperties.PRIMARY_COLOR, -1);

        mThemeColorObserverArgumentCaptor.getValue().onThemeColorChanged(1, false);

        assertThat(mModel.get(TabStripToolbarViewProperties.PRIMARY_COLOR), equalTo(1));
    }

    @Test
    public void tintChange() {
        initAndAssertProperties(mTab1);
        mModel.set(TabStripToolbarViewProperties.TINT, null);
        ColorStateList colorStateList = mock(ColorStateList.class);

        mTintObserverArgumentCaptor.getValue().onTintChanged(colorStateList, true);

        assertThat(mModel.get(TabStripToolbarViewProperties.TINT), equalTo(colorStateList));
    }

    @Test
    public void backButtonPress_ShouldHandle() {
        initAndAssertProperties(mTab1);
        doReturn(true).when(mTabGridDialogController).handleBackPressed();

        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(true));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    public void backButtonPress_ShouldNotHandle() {
        initAndAssertProperties(mTab1);
        doReturn(false).when(mTabGridDialogController).handleBackPressed();

        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(false));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    public void destroy() {
        initAndAssertProperties(mTab1);

        mTabGroupUiMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());
        verify(mOverviewModeBehavior)
                .removeOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());
        verify(mThemeColorProvider)
                .removeThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        verify(mThemeColorProvider).removeTintObserver(mTintObserverArgumentCaptor.capture());
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverArgumentCaptor.capture());
    }

    @Test
    public void uiNotVisibleAfterDragCurrentTabOutOfGroup() {
        initAndAssertProperties(mTab3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab3));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabGroupModelFilterObserverArgumentCaptor.getValue().didMoveTabOutOfGroup(mTab3, 1);

        verifyResetStrip(false, null);
    }
}