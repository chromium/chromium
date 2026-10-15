// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link CloseAllTabsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CloseAllTabsHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabModel mArchivedTabModel;
    @Mock private TabRemover mRegularTabRemover;
    @Mock private TabRemover mIncognitoTabRemover;
    @Mock private Profile mProfile;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabArchiver mTabArchiver;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTabCreatorManager()).thenReturn(mTabCreatorManager);
        when(mRegularTabModel.getTabRemover()).thenReturn(mRegularTabRemover);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);

        // Setup deps for tab archiving.
        when(mTabModelSelector.getCurrentModel()).thenReturn(mRegularTabModel);
        when(mRegularTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mArchivedTabModelOrchestrator.getTabArchiver()).thenReturn(mTabArchiver);
        when(mArchivedTabModelOrchestrator.getTabModel()).thenReturn(mArchivedTabModel);
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
    }

    @Test
    public void testCloseAllTabsHidingTabGroups_AllowUndo() {
        testCloseAllTabsHidingTabGroups(/* shouldAllowUndo= */ true);
    }

    @Test
    public void testCloseAllTabsHidingTabGroups_DisallowUndo() {
        testCloseAllTabsHidingTabGroups(/* shouldAllowUndo= */ false);
    }

    private void testCloseAllTabsHidingTabGroups(boolean shouldAllowUndo) {
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector, shouldAllowUndo, /* closePinnedTabs= */ false);

        ArgumentMatcher<TabClosureParams> tabClosureParamsMatcher =
                params -> params.isAllTabs && (params.allowUndo == shouldAllowUndo);
        verify(mRegularTabRemover)
                .closeTabs(argThat(tabClosureParamsMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(tabClosureParamsMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    public void testBuildCloseAllTabsCallback_Regular_AllowUndo() {
        testBuildCloseAllTabsCallback(/* isIncognitoOnly= */ false, /* allowUndo= */ true);
    }

    @Test
    public void testBuildCloseAllTabsCallback_Regular_DisAllowUndo() {
        testBuildCloseAllTabsCallback(/* isIncognitoOnly= */ false, /* allowUndo= */ false);
    }

    @Test
    public void testBuildCloseAllTabsCallback_Incognito_AllowUndo() {
        testBuildCloseAllTabsCallback(/* isIncognitoOnly= */ true, /* allowUndo= */ true);
    }

    @Test
    public void testBuildCloseAllTabsCallback_Incognito_DisAllowUndo() {
        testBuildCloseAllTabsCallback(/* isIncognitoOnly= */ true, /* allowUndo= */ false);
    }

    private void testBuildCloseAllTabsCallback(boolean isIncognitoOnly, boolean allowUndo) {
        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, isIncognitoOnly, allowUndo);

        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> tabClosureParamsMatcher =
                params -> params.isAllTabs && (params.allowUndo == allowUndo);
        if (isIncognitoOnly) {
            verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
            verify(mIncognitoTabRemover)
                    .closeTabs(argThat(tabClosureParamsMatcher), /* allowDialog= */ eq(false));
        } else {
            verify(mRegularTabRemover)
                    .closeTabs(argThat(tabClosureParamsMatcher), /* allowDialog= */ eq(false));
            verify(mIncognitoTabRemover)
                    .closeTabs(argThat(tabClosureParamsMatcher), /* allowDialog= */ eq(false));
        }
    }

    @Test
    public void testUninitializedArchivedTabModelReturnsEmptyRunnable() {
        when(mArchivedTabModelOrchestrator.areTabModelsInitialized()).thenReturn(false);
        Runnable r =
                CloseAllTabsHelper.removeArchivedTabsAndGetUndoRunnable(
                        mArchivedTabModelOrchestrator, mTabModelSelector);
        assertEquals(CallbackUtils.emptyRunnable(), r);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_RetainsPinnedTabsWhenUnchecked() {
        Tab pinnedTab = mock(Tab.class);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        Tab unpinnedTab = mock(Tab.class);
        when(unpinnedTab.getIsPinned()).thenReturn(false);

        when(mRegularTabModel.getPinnedTabsCount()).thenReturn(1);
        when(mRegularTabModel.getCount()).thenReturn(2);
        when(mRegularTabModel.getTabAt(0)).thenReturn(pinnedTab);
        when(mRegularTabModel.getTabAt(1)).thenReturn(unpinnedTab);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, /* isIncognitoOnly= */ false, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        !params.isAllTabs
                                && List.of(unpinnedTab).equals(params.tabs)
                                && params.allowUndo
                                && params.hideTabGroups;
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        InOrder inOrder = inOrder(mArchivedTabModelOrchestrator, mRegularTabModel);
        inOrder.verify(mArchivedTabModelOrchestrator).areTabModelsInitialized();
        inOrder.verify(mRegularTabModel).getTabAt(0);

        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_OnlyPinnedTabs_SkipsRegularTabRemover() {
        Tab pinnedTab1 = mock(Tab.class);
        when(pinnedTab1.getIsPinned()).thenReturn(true);
        Tab pinnedTab2 = mock(Tab.class);
        when(pinnedTab2.getIsPinned()).thenReturn(true);

        when(mRegularTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mRegularTabModel.getCount()).thenReturn(2);
        when(mRegularTabModel.getTabAt(0)).thenReturn(pinnedTab1);
        when(mRegularTabModel.getTabAt(1)).thenReturn(pinnedTab2);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, /* isIncognitoOnly= */ false, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mArchivedTabModelOrchestrator).areTabModelsInitialized();
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_OnlyPinnedTabsWithArchivedTabs_ClosesArchivedTabs() {
        Tab pinnedTab = mock(Tab.class);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        Tab unarchivedTab = mock(Tab.class);
        when(unarchivedTab.getIsPinned()).thenReturn(false);

        List<Tab> regularTabs = new ArrayList<>(List.of(pinnedTab));
        when(mRegularTabModel.getPinnedTabsCount()).thenReturn(1);
        when(mRegularTabModel.getCount()).thenAnswer(inv -> regularTabs.size());
        when(mRegularTabModel.getTabAt(anyInt()))
                .thenAnswer(inv -> regularTabs.get(inv.getArgument(0)));

        when(mArchivedTabModelOrchestrator.areTabModelsInitialized()).thenReturn(true);
        when(mArchivedTabModel.getCount()).thenReturn(1);
        when(mArchivedTabModel.iterator()).thenAnswer(inv -> List.of(unarchivedTab).iterator());
        doAnswer(
                        inv -> {
                            regularTabs.add(unarchivedTab);
                            return null;
                        })
                .when(mTabArchiver)
                .unarchiveAndRestoreTabs(any(), eq(List.of(unarchivedTab)), eq(true), eq(false));

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, /* isIncognitoOnly= */ false, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        !params.isAllTabs
                                && List.of(unarchivedTab).equals(params.tabs)
                                && params.allowUndo
                                && params.hideTabGroups;
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_ClosesAllWhenChecked() {
        when(mRegularTabModel.getPinnedTabsCount()).thenReturn(1);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, /* isIncognitoOnly= */ false, /* allowUndo= */ true);
        callback.onResult(/* result= */ true);

        ArgumentMatcher<TabClosureParams> allTabsMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover).closeTabs(argThat(allTabsMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(allTabsMatcher), /* allowDialog= */ eq(false));
    }
}
