// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.nullValue;
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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Assert;
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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupUiMediator}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument", "unchecked"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
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
    TabCreator mTabCreator;
    @Mock
    LayoutStateProvider mLayoutManager;
    @Mock
    IncognitoStateProvider mIncognitoStateProvider;
    @Mock
    TabModel mTabModel;
    @Mock
    View mView;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabGridDialogMediator.DialogController mTabGridDialogController;
    @Mock
    Context mContext;
    @Mock
    SnackbarManager mSnackbarManager;
    @Mock
    ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    @Mock
    private Resources mResources;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<IncognitoStateObserver> mIncognitoStateObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    ArgumentCaptor<Callback<Boolean>> mOmniboxFocusObserverCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private List<Tab> mTabGroup1;
    private List<Tab> mTabGroup2;
    private List<Tab> mAllTabsList;
    private PropertyModel mModel;
    private TabGroupUiMediator mTabGroupUiMediator;
    private InOrder mResetHandlerInOrder;
    private InOrder mVisibilityControllerInOrder;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mTabGridDialogBackPressSupplier =
            new ObservableSupplierImpl<>();

    private TabImpl prepareTab(int tabId, int rootId) {
        TabImpl tab = TabUiUnitTestUtils.prepareTab(tabId, rootId);
        doReturn(tab).when(mTabModelSelector).getTabById(tabId);
        return tab;
    }

    private TabModel prepareIncognitoTabModel() {
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
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

        TabGridDialogMediator.DialogController controller =
                TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext) ? mTabGridDialogController
                                                                          : null;
        OneshotSupplierImpl<TabGridDialogMediator.DialogController> controllerSupplier =
                new OneshotSupplierImpl<>();
        doReturn(mTabGridDialogBackPressSupplier)
                .when(mTabGridDialogController)
                .getHandleBackPressChangedSupplier();
        controllerSupplier.set(controller);
        mTabGroupUiMediator = new TabGroupUiMediator(mContext, mVisibilityController, mResetHandler,
                mModel, mTabModelSelector, mTabCreatorManager, mLayoutStateProviderSupplier,
                mIncognitoStateProvider, controllerSupplier, mOmniboxFocusStateSupplier);

        if (currentTab == null) {
            verifyNeverReset();
            return;
        }

        // Verify strip button content description setup.
        verify(mContext).getString(R.string.accessibility_bottom_tab_strip_expand_tab_sheet);
        verify(mContext).getString(R.string.bottom_tab_grid_new_tab);

        // Verify strip initial reset.
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(currentTab.getId());
        if (tabs.size() < 2) {
            verifyResetStrip(false, null);
        } else {
            verifyResetStrip(true, tabs);
        }
    }

    @Before
    public void setUp() {
        // After setUp(), tabModel has 3 tabs in the following order: mTab1, mTab2 and mTab3. If
        // TabGroup is enabled, mTab2 and mTab3 are in a group. Each test must call
        // initAndAssertProperties(selectedTab) first, with selectedTab being the currently selected
        // tab when the TabGroupUiMediator is created.
        MockitoAnnotations.initMocks(this);

        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(1);

        // Set up Tabs
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID);
        mTabGroup1 = new ArrayList<>(Arrays.asList(mTab1));
        mTabGroup2 = new ArrayList<>(Arrays.asList(mTab2, mTab3));
        mAllTabsList = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        // Setup TabModel.
        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doReturn(3).when(mTabModel).getCount();
        doReturn(0).when(mTabModel).index();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab3).when(mTabModel).getTabAt(2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab2).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab3).addObserver(mTabObserverCaptor.capture());

        // Setup TabGroupModelFilter.
        doReturn(false).when(mTabGroupModelFilter).isIncognito();
        doReturn(2).when(mTabGroupModelFilter).getCount();
        doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());

        // Set up TabModelSelector and TabModelFilterProvider.
        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doNothing()
                .when(mTabModelSelector)
                .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        doReturn(tabModelList).when(mTabModelSelector).getModels();

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());


        // Set up OverviewModeBehavior
        doNothing().when(mLayoutManager).addObserver(mLayoutStateObserverCaptor.capture());
        mLayoutStateProviderSupplier.set(mLayoutManager);

        // Set up IncognitoStateProvider
        doNothing()
                .when(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(
                        mIncognitoStateObserverArgumentCaptor.capture());

        // Set up ResetHandler
        doNothing().when(mResetHandler).resetStripWithListOfTabs(any());
        doNothing().when(mResetHandler).resetGridWithListOfTabs(any());

        // Set up TabCreatorManager
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(null).when(mTabCreator).createNewTab(any(), anyInt(), any());

        // Set up omnibox focus state observer.
        doReturn(nullValue())
                .when(mOmniboxFocusStateSupplier)
                .addObserver(mOmniboxFocusObserverCaptor.capture());

        mResetHandlerInOrder = inOrder(mResetHandler);
        mVisibilityControllerInOrder = inOrder(mVisibilityController);
        mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
    }

    /*********************** Tab group related tests *************************/

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_NoTab_TabGroup() {
        initAndAssertProperties(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_SingleTab() {
        initAndAssertProperties(mTab1);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_TabGroup() {
        // Tab 2 is in a tab group.
        initAndAssertProperties(mTab2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void onClickLeftButton_TabGroup() {
        initAndAssertProperties(mTab2);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mResetHandler).resetGridWithListOfTabs(mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void onClickRightButton_TabGroup() {
        initAndAssertProperties(mTab1);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTab1));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_GroupToSingleTab() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be showing since tab 1 is a single tab.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_GroupToGroup() {
        initAndAssertProperties(mTab2);

        // Mock that tab 1 is not a single tab.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should be showing since we are selecting a group.
        verifyResetStrip(true, tabs);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_SingleTabToGroup() {
        initAndAssertProperties(mTab1);

        // Mock that tab 2 is not a single tab.
        List<Tab> tabGroup = mTabGroupModelFilter.getRelatedTabList(TAB2_ID);
        assertThat(tabGroup.size(), equalTo(2));

        // Mock selecting tab 2, and the last selected tab is tab 1 which is a single tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should be showing since we are selecting a group.
        verifyResetStrip(true, tabGroup);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_SingleTabToSingleTab() {
        initAndAssertProperties(mTab1);

        // Mock that new tab is a single tab.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        // Mock selecting new tab, and the last selected tab is tab 1 which is also a single tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                newTab, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should not be showing since new tab is a single tab.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_SameGroup_TabGroup() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 3, and the last selected tab is tab 2 which is in the same group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be reset since we are selecting in one group.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_ScrollToSelectedIndex() {
        initAndAssertProperties(mTab1);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(null));

        // Mock that {tab2, tab3} are in the same tab group.
        List<Tab> tabGroup = mTabGroupModelFilter.getRelatedTabList(TAB2_ID);
        assertThat(tabGroup.size(), equalTo(2));

        // Mock selecting tab 3, and the last selected tab is tab 1 which is a single tab.
        doReturn(mTab3).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should be showing since we are selecting a group, and it should scroll to the index
        // of currently selected tab.
        verifyResetStrip(true, tabGroup);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(1));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosure_NotLastTabInGroup() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2, and tab 3 then gets selected. They are in the same group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);

        // Strip should not be reset since we are still in this group.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosure_LastTabInGroup_GroupUiNotVisible() {
        initAndAssertProperties(mTab1);

        // Mock closing tab 1, and tab 2 then gets selected. They are in different group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab1, true, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_CLOSE, TAB1_ID);

        // Strip should be reset since we are switching to a different group.
        verifyResetStrip(true, mTabGroup2);
    }

    // TODO(988199): Ignore this test until we have a conclusion from the attached bug.
    @Ignore
    @Test
    public void tabClosure_LastTabInGroup_GroupUiVisible() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2 and tab, then tab 1 gets selected. They are in different group. Right
        // now tab group UI is visible.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab3, true, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_CLOSE, TAB3_ID);

        // Strip should be reset since tab group UI was visible and now we are switching to a
        // different group.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_SingleTab() {
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE, false);

        // Strip should be not be reset when adding a single new tab.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_SingleTab_Refresh_WithAutoGroupCreation() {
        TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.setForTesting(true);
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_FOREGROUND,
                false);

        // Strip should be be reset when long pressing a link and add a tab into group.
        verifyResetStrip(true, tabs);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_SingleTab_Refresh_WithoutAutoGroupCreation() {
        TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.setForTesting(false);
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                TabCreationState.LIVE_IN_FOREGROUND, false);

        // Strip should be be reset when long pressing a link and add a tab into group.
        verifyResetStrip(true, tabs);

        // The default value is true. Reset back to the default value.
        TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.setForTesting(true);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_TabGroup_NoRefresh() {
        initAndAssertProperties(mTab2);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE, false);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_FOREGROUND,
                false);

        // Strip should be not be reset through these two types of launching.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_TabGroup_ScrollToTheLast() {
        initAndAssertProperties(mTab2);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(0));

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        // Strip should be not be reset through adding tab from UI.
        verifyNeverReset();
        assertThat(mTabGroupModelFilter.getRelatedTabList(TAB4_ID).size(), equalTo(3));
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(2));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_TabModelNoTab() {
        // Simulate no tab in current TabModel.
        initAndAssertProperties(null);

        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_OverviewModeVisible() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3. Also, the overview
        // mode is visible when restoring completed.
        initAndAssertProperties(mTab2);
        doReturn(POSITION2).when(mTabModel).index();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        doReturn(true).when(mLayoutManager).isLayoutVisible(LayoutType.TAB_SWITCHER);
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiVisible_NotShowingOverviewMode() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(mTab2);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that another member of this group, newTab, is being undone from closure.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab2, mTab3, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Since the strip is already visible, no resetting.
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_NotShowingOverviewMode_TabNotInGroup() {
        // Assume mTab1 is selected. Since mTab1 is now a single tab, the strip is invisible.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate mTab2 and mTab3 being undone from closure with mTab1 still selected.
        doReturn(new ArrayList<>(Arrays.asList(mTab2, mTab3)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab2, mTab3)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB3_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab2);
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);

        // Strip should remain invisible.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_NotShowingOverviewMode_TabInGroup() {
        // Assume mTab1 is selected. Since mTab1 is now a single tab, the strip is invisible.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that newTab which was a tab in the same group as mTab1 is being undone from
        // closure.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Strip should reset to be visible.
        mVisibilityControllerInOrder.verify(mVisibilityController)
                .setBottomControlsVisible(eq(true));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_ShowingTabSwitcherMode() {
        tabClosureUndone_UiNotVisible_ShowingOverviewModeImpl(LayoutType.TAB_SWITCHER);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_ShowingStartSurface() {
        tabClosureUndone_UiNotVisible_ShowingOverviewModeImpl(LayoutType.START_SURFACE);
    }

    private void tabClosureUndone_UiNotVisible_ShowingOverviewModeImpl(@LayoutType int layoutType) {
        // Assume mTab1 is selected.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate the overview mode is showing, which hides the strip.
        mLayoutStateObserverCaptor.getValue().onStartedShowing(layoutType);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(true));
        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);

        // Simulate that we undo a group closure of {mTab2, mTab3}.
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab2);

        // Since overview mode is showing, we should not show strip.
        mVisibilityControllerInOrder.verify(mVisibilityController, times(2))
                .setBottomControlsVisible(eq(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewStartedShowing() {
        overViewStartedShowingImpl(LayoutType.TAB_SWITCHER);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewStartedShowing_StartSurface() {
        overViewStartedShowingImpl(LayoutType.START_SURFACE);
    }

    private void overViewStartedShowingImpl(@LayoutType int layoutType) {
        initAndAssertProperties(mTab1);

        mLayoutStateObserverCaptor.getValue().onStartedShowing(layoutType);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_NoCurrentTab() {
        overViewFinishedHiding_NoCurrentTabImpl(LayoutType.TAB_SWITCHER);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_NoCurrentTab_StartSurface() {
        overViewFinishedHiding_NoCurrentTabImpl(LayoutType.START_SURFACE);
    }

    private void overViewFinishedHiding_NoCurrentTabImpl(@LayoutType int layoutType) {
        initAndAssertProperties(null);

        mLayoutStateObserverCaptor.getValue().onFinishedHiding(layoutType);

        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabSingle() {
        overViewFinishedHiding_CurrentTabSingleImpl(LayoutType.TAB_SWITCHER);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabSingle_StartSurface() {
        overViewFinishedHiding_CurrentTabSingleImpl(LayoutType.START_SURFACE);
    }

    private void overViewFinishedHiding_CurrentTabSingleImpl(@LayoutType int layoutType) {
        initAndAssertProperties(mTab1);

        mLayoutStateObserverCaptor.getValue().onFinishedHiding(layoutType);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabInGroup() {
        overViewFinishedHiding_CurrentTabInGroupImpl(LayoutType.TAB_SWITCHER);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabInGroup_StartSurface() {
        overViewFinishedHiding_CurrentTabInGroupImpl(LayoutType.START_SURFACE);
    }

    public void overViewFinishedHiding_CurrentTabInGroupImpl(@LayoutType int layoutType) {
        initAndAssertProperties(mTab2);

        mLayoutStateObserverCaptor.getValue().onFinishedHiding(layoutType);

        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void destroy_TabGroup() {
        initAndAssertProperties(mTab1);

        mTabGroupUiMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());
        verify(mLayoutManager).removeObserver(mLayoutStateObserverCaptor.capture());
        verify(mIncognitoStateProvider)
                .removeObserver(mIncognitoStateObserverArgumentCaptor.capture());
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        verify(mTabGroupModelFilter, times(2))
                .removeTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void uiNotVisibleAfterDragCurrentTabOutOfGroup() {
        initAndAssertProperties(mTab3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab3));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabGroupModelFilterObserverArgumentCaptor.getValue().didMoveTabOutOfGroup(mTab3, 1);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void backButtonPress_ShouldHandle() {
        initAndAssertProperties(mTab1);
        doReturn(true).when(mTabGridDialogController).handleBackPressed();
        mTabGridDialogBackPressSupplier.set(true);

        var groupUiBackPressSupplier = mTabGroupUiMediator.getHandleBackPressChangedSupplier();
        Assert.assertEquals(Boolean.TRUE, groupUiBackPressSupplier.get());

        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(true));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void backButtonPress_ShouldNotHandle() {
        initAndAssertProperties(mTab1);
        doReturn(false).when(mTabGridDialogController).handleBackPressed();
        mTabGridDialogBackPressSupplier.set(false);
        var groupUiBackPressSupplier = mTabGroupUiMediator.getHandleBackPressChangedSupplier();

        Assert.assertNotEquals(Boolean.TRUE, groupUiBackPressSupplier.get());
        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(false));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void backButtonPress_LateInitController() {
        initAndAssertProperties(mTab1);
        TabGridDialogMediator.DialogController controller = mTabGridDialogController;
        var backPressSupplier = new ObservableSupplierImpl<Boolean>();
        doReturn(backPressSupplier)
                .when(mTabGridDialogController)
                .getHandleBackPressChangedSupplier();
        OneshotSupplierImpl<TabGridDialogMediator.DialogController> controllerSupplier =
                new OneshotSupplierImpl<>();
        mTabGroupUiMediator = new TabGroupUiMediator(mContext, mVisibilityController, mResetHandler,
                mModel, mTabModelSelector, mTabCreatorManager, mLayoutStateProviderSupplier,
                mIncognitoStateProvider, controllerSupplier, mOmniboxFocusStateSupplier);

        var groupUiBackPressSupplier = mTabGroupUiMediator.getHandleBackPressChangedSupplier();

        // Not initialized yet.
        Assert.assertNotEquals(Boolean.TRUE, groupUiBackPressSupplier.get());

        // Late init.
        controllerSupplier.set(controller);
        doReturn(false).when(mTabGridDialogController).handleBackPressed();
        backPressSupplier.set(false);

        Assert.assertFalse(groupUiBackPressSupplier.get());

        backPressSupplier.set(true);
        doReturn(true).when(mTabGridDialogController).handleBackPressed();
        Assert.assertTrue(groupUiBackPressSupplier.get());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void switchTabModel_UiVisible_TabGroup() {
        initAndAssertProperties(mTab1);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
        TabModel incognitoTabModel = prepareIncognitoTabModel();

        // Mock that tab2 is selected after tab model switch, and tab2 is in a group.
        doReturn(TAB2_ID).when(mTabModelSelector).getCurrentTabId();
        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void switchTabModel_UiNotVisible_TabGroup() {
        initAndAssertProperties(mTab1);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
        TabModel incognitoTabModel = prepareIncognitoTabModel();

        // Mock that tab1 is selected after tab model switch, and tab1 is a single tab.
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verifyResetStrip(false, null);
    }

    /*********************** Class common tests *************************/

    @Test
    public void incognitoChange() {
        initAndAssertProperties(mTab1);
        mModel.set(TabGroupUiProperties.IS_INCOGNITO, false);

        mIncognitoStateObserverArgumentCaptor.getValue().onIncognitoStateChanged(true);

        assertThat(mModel.get(TabGroupUiProperties.IS_INCOGNITO), equalTo(true));
    }

    @Test
    public void testSetLeftButtonDrawable() {
        initAndAssertProperties(mTab3);
        int drawableId = 321;

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, 0);

        mTabGroupUiMediator.setupLeftButtonDrawable(drawableId);

        assertThat(mModel.get(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID), equalTo(drawableId));
    }

    @Test
    public void testSetLeftButtonOnClickListener() {
        initAndAssertProperties(mTab3);
        View.OnClickListener listener = v -> {};

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, null);

        mTabGroupUiMediator.setupLeftButtonOnClickListener(listener);

        assertThat(
                mModel.get(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER), equalTo(listener));
    }

    @Test
    public void testTabModelSelectorTabObserverDestroyWhenDetach() {
        InOrder tabObserverDestroyInOrder = inOrder(mTab1);
        initAndAssertProperties(mTab1);

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab1, null);

        tabObserverDestroyInOrder.verify(mTab1).removeObserver(mTabObserverCaptor.capture());

        mTabGroupUiMediator.destroy();

        tabObserverDestroyInOrder.verify(mTab1, never())
                .removeObserver(mTabObserverCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testOmniboxFocusChange() {
        initAndAssertProperties(mTab2);

        mOmniboxFocusObserverCaptor.getValue().onResult(true);
        verifyResetStrip(false, null);

        doReturn(TAB2_ID).when(mTabModelSelector).getCurrentTabId();
        mOmniboxFocusObserverCaptor.getValue().onResult(false);
        verifyResetStrip(true, mTabGroup2);
    }
}
