// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.close_all_tabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.StubTabModel;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.List;
import java.util.function.Supplier;

/** Unit test for {@link CloseAllTabsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CloseAllTabsHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabRemover mRegularTabRemover;
    @Mock private TabRemover mIncognitoTabRemover;
    @Mock private Profile mProfile;
    @Mock private Supplier<Runnable> mUnarchiveAndGetUndoRunnable;
    @Mock private Runnable mUndoRunnable;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);
    }

    private void setUpCloseAllTabsMocks() {
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mRegularTabModel.getTabRemover()).thenReturn(mRegularTabRemover);
        when(mUnarchiveAndGetUndoRunnable.get()).thenReturn(mUndoRunnable);
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
        setUpCloseAllTabsMocks();
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mUnarchiveAndGetUndoRunnable,
                shouldAllowUndo,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        params.isAllTabs
                                && (params.allowUndo == shouldAllowUndo)
                                && (shouldAllowUndo
                                        ? params.undoRunnable == mUndoRunnable
                                        : params.undoRunnable == null);
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && (params.allowUndo == shouldAllowUndo);
        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    public void testBuildCloseAllTabsCallback_Regular_AllowUndo() {
        testBuildCloseAllTabsCallback(/* allowUndo= */ true);
    }

    @Test
    public void testBuildCloseAllTabsCallback_Regular_DisallowUndo() {
        testBuildCloseAllTabsCallback(/* allowUndo= */ false);
    }

    @Test
    public void testBuildCloseAllIncognitoTabsCallback_AllowUndo() {
        testBuildCloseAllIncognitoTabsCallback(/* allowUndo= */ true);
    }

    @Test
    public void testBuildCloseAllIncognitoTabsCallback_DisallowUndo() {
        testBuildCloseAllIncognitoTabsCallback(/* allowUndo= */ false);
    }

    private void testBuildCloseAllTabsCallback(boolean allowUndo) {
        setUpCloseAllTabsMocks();
        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mUnarchiveAndGetUndoRunnable, allowUndo);

        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        params.isAllTabs
                                && (params.allowUndo == allowUndo)
                                && (allowUndo
                                        ? params.undoRunnable == mUndoRunnable
                                        : params.undoRunnable == null);
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && (params.allowUndo == allowUndo);
        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    private void testBuildCloseAllIncognitoTabsCallback(boolean allowUndo) {
        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllIncognitoTabsCallback(mTabModelSelector, allowUndo);

        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && (params.allowUndo == allowUndo);
        verify(mUnarchiveAndGetUndoRunnable, never()).get();
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_RetainsPinnedTabsWhenUnchecked() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab pinnedTab = regularTabModel.addTab(1);
        pinnedTab.setIsPinned(true);
        MockTab unpinnedTab = regularTabModel.addTab(2);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        when(mUnarchiveAndGetUndoRunnable.get()).thenReturn(mUndoRunnable);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mUnarchiveAndGetUndoRunnable, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        !params.isAllTabs
                                && List.of(unpinnedTab).equals(params.tabs)
                                && params.allowUndo
                                && params.hideTabGroups
                                && params.undoRunnable == mUndoRunnable;
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_OnlyPinnedTabs_SkipsRegularTabRemover() {
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mUnarchiveAndGetUndoRunnable.get()).thenReturn(mUndoRunnable);
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
                        mTabModelSelector, mUnarchiveAndGetUndoRunnable, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mUnarchiveAndGetUndoRunnable).get();
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_OnlyPinnedTabsWithArchivedTabs_ClosesArchivedTabs() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab pinnedTab = regularTabModel.addTab(1);
        pinnedTab.setIsPinned(true);
        MockTab unarchivedTab = new MockTab(2, mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector,
                        () -> {
                            regularTabModel.addTab(
                                    unarchivedTab,
                                    /* index= */ 0,
                                    TabLaunchType.FROM_RESTORE,
                                    TabCreationState.FROZEN_ON_RESTORE);
                            return mUndoRunnable;
                        },
                        /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        !params.isAllTabs
                                && List.of(unarchivedTab).equals(params.tabs)
                                && params.allowUndo
                                && params.hideTabGroups
                                && params.undoRunnable == mUndoRunnable;
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testBuildCloseAllTabsCallback_ClosesAllWhenChecked() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab pinnedTab = regularTabModel.addTab(1);
        pinnedTab.setIsPinned(true);
        regularTabModel.addTab(2);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        when(mUnarchiveAndGetUndoRunnable.get()).thenReturn(mUndoRunnable);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mUnarchiveAndGetUndoRunnable, /* allowUndo= */ true);
        callback.onResult(/* result= */ true);

        ArgumentMatcher<TabClosureParams> regularMatcher =
                params ->
                        params.isAllTabs
                                && params.allowUndo
                                && params.hideTabGroups
                                && params.undoRunnable == mUndoRunnable;
        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover).closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
        verify(mIncognitoTabRemover)
                .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
    }

    @Test
    public void testCloseAllTabsWithStubRegularModel() {
        StubTabModel stubRegularModel = new StubTabModel(/* isIncognito= */ false, mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(stubRegularModel);

        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mUnarchiveAndGetUndoRunnable,
                /* allowUndo= */ false,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mIncognitoTabRemover).closeTabs(any(), eq(false));
        verify(mUnarchiveAndGetUndoRunnable, never()).get();
    }

    @Test
    public void testCloseAllTabsWithStubIncognitoModel() {
        setUpCloseAllTabsMocks();
        StubTabModel stubIncognitoModel =
                new StubTabModel(/* isIncognito= */ true, /* profile= */ null);
        when(mTabModelSelector.getModel(true)).thenReturn(stubIncognitoModel);

        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mUnarchiveAndGetUndoRunnable,
                /* allowUndo= */ false,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mRegularTabRemover).closeTabs(any(), eq(false));
    }
}
