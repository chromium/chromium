// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.close_all_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.close_all_tabs.CloseAllTabsHelper.ArchivedTabsDelegate;
import org.chromium.chrome.browser.tabmodel.StubTabModel;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

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
    @Mock private ArchivedTabsDelegate mArchivedTabsDelegate;
    @Mock private Runnable mUndoRunnable;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);
        when(mArchivedTabsDelegate.prepareForCloseAll(any())).thenReturn(null);
    }

    @After
    public void tearDown() {
        CloseAllTabsHelper.clearPendingCloseAllSelectorsForTesting();
    }

    private void setUpCloseAllTabsMocks() {
        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mRegularTabModel.getTabRemover()).thenReturn(mRegularTabRemover);
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable()).thenReturn(mUndoRunnable);
    }

    @Test
    public void testCloseAllTabsHidingTabGroups() {
        setUpCloseAllTabsMocks();
        for (boolean shouldAllowUndo : new boolean[] {true, false}) {
            CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                    mTabModelSelector,
                    mArchivedTabsDelegate,
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
            verify(mRegularTabRemover)
                    .closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
            verify(mIncognitoTabRemover)
                    .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
            clearInvocations(mRegularTabRemover, mIncognitoTabRemover);
        }
    }

    @Test
    public void testBuildCloseAllTabsCallback_Regular() {
        setUpCloseAllTabsMocks();
        for (boolean allowUndo : new boolean[] {true, false}) {
            Callback<Boolean> callback =
                    CloseAllTabsHelper.buildCloseAllTabsCallback(
                            mTabModelSelector, mArchivedTabsDelegate, allowUndo);

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
            verify(mRegularTabRemover)
                    .closeTabs(argThat(regularMatcher), /* allowDialog= */ eq(false));
            verify(mIncognitoTabRemover)
                    .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
            clearInvocations(mRegularTabRemover, mIncognitoTabRemover);
        }
    }

    @Test
    public void testBuildCloseAllIncognitoTabsCallback() {
        for (boolean allowUndo : new boolean[] {true, false}) {
            Callback<Boolean> callback =
                    CloseAllTabsHelper.buildCloseAllIncognitoTabsCallback(
                            mTabModelSelector, allowUndo);

            callback.onResult(/* result= */ false);

            ArgumentMatcher<TabClosureParams> incognitoMatcher =
                    params -> params.isAllTabs && (params.allowUndo == allowUndo);
            verify(mArchivedTabsDelegate, never()).prepareForCloseAll(any());
            verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();
            verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
            verify(mIncognitoTabRemover)
                    .closeTabs(argThat(incognitoMatcher), /* allowDialog= */ eq(false));
            clearInvocations(mIncognitoTabRemover);
        }
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
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable()).thenReturn(mUndoRunnable);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mArchivedTabsDelegate, /* allowUndo= */ true);
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
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab pinnedTab1 = regularTabModel.addTab(1);
        pinnedTab1.setIsPinned(true);
        MockTab pinnedTab2 = regularTabModel.addTab(2);
        pinnedTab2.setIsPinned(true);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable()).thenReturn(mUndoRunnable);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mArchivedTabsDelegate, /* allowUndo= */ true);
        callback.onResult(/* result= */ false);

        ArgumentMatcher<TabClosureParams> incognitoMatcher =
                params -> params.isAllTabs && params.allowUndo && params.hideTabGroups;

        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mArchivedTabsDelegate).unarchiveAndGetUndoRunnable();
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
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable())
                .thenAnswer(
                        _ -> {
                            regularTabModel.addTab(
                                    unarchivedTab,
                                    /* index= */ 0,
                                    TabLaunchType.FROM_RESTORE,
                                    TabCreationState.FROZEN_ON_RESTORE);
                            return mUndoRunnable;
                        });

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mArchivedTabsDelegate, /* allowUndo= */ true);
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
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable()).thenReturn(mUndoRunnable);

        Callback<Boolean> callback =
                CloseAllTabsHelper.buildCloseAllTabsCallback(
                        mTabModelSelector, mArchivedTabsDelegate, /* allowUndo= */ true);
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
                mArchivedTabsDelegate,
                /* allowUndo= */ false,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mIncognitoTabRemover).closeTabs(any(), eq(false));
        verify(mArchivedTabsDelegate, never()).prepareForCloseAll(any());
        verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();
    }

    @Test
    public void testCloseAllTabsWithStubIncognitoModel() {
        setUpCloseAllTabsMocks();
        StubTabModel stubIncognitoModel =
                new StubTabModel(/* isIncognito= */ true, /* profile= */ null);
        when(mTabModelSelector.getModel(true)).thenReturn(stubIncognitoModel);

        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mArchivedTabsDelegate,
                /* allowUndo= */ false,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mRegularTabRemover).closeTabs(any(), eq(false));
    }

    @Test
    public void testCloseAllTabs_DeferredReadyPreservesInterimTabsAndDeduplicates() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab initialTab = regularTabModel.addTab(1);
        MockTab interimTab = new MockTab(99, mProfile);
        MockTab archivedTab = new MockTab(42, mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);

        Destroyable preparationHandle = mock(Destroyable.class);
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable())
                .thenAnswer(
                        _ -> {
                            regularTabModel.addTab(
                                    archivedTab,
                                    /* index= */ 0,
                                    TabLaunchType.FROM_RESTORE,
                                    TabCreationState.FROZEN_ON_RESTORE);
                            return mUndoRunnable;
                        });

        Runnable readyCallback = startPendingCloseAll(/* allowUndo= */ true, preparationHandle);

        // Incognito closes synchronously up front; regular close is deferred.
        verify(mIncognitoTabRemover, times(1)).closeTabs(any(), eq(false));
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();

        // Concurrent request for the same selector is ignored while pending.
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mArchivedTabsDelegate,
                /* allowUndo= */ true,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mIncognitoTabRemover, times(1)).closeTabs(any(), eq(false));
        verify(mArchivedTabsDelegate, times(1)).prepareForCloseAll(any());

        // Interim tab opened during async wait is preserved when archived tabs are unarchived.
        regularTabModel.addTab(
                interimTab,
                /* index= */ 1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        readyCallback.run();

        ArgumentCaptor<TabClosureParams> paramsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mRegularTabRemover).closeTabs(paramsCaptor.capture(), eq(false));
        TabClosureParams params = paramsCaptor.getValue();
        assertFalse(params.isAllTabs);
        assertEquals(Set.of(initialTab, archivedTab), new HashSet<>(params.tabs));
        assertSame(mUndoRunnable, params.undoRunnable);
        verify(preparationHandle, times(1)).destroy();
    }

    @Test
    public void testCloseAllTabs_DeferredReadyTargetsGoneWithInterimTab_SkipsRegularClose() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        MockTab initialTab = regularTabModel.addTab(1);
        MockTab interimTab = new MockTab(99, mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);

        Destroyable preparationHandle = mock(Destroyable.class);
        when(mArchivedTabsDelegate.unarchiveAndGetUndoRunnable()).thenReturn(mUndoRunnable);

        Runnable readyCallback = startPendingCloseAll(/* allowUndo= */ false, preparationHandle);

        regularTabModel.removeTab(initialTab);
        regularTabModel.addTab(
                interimTab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        readyCallback.run();

        verify(mArchivedTabsDelegate).unarchiveAndGetUndoRunnable();
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    @Test
    public void
            testCloseAllTabs_SelectorDestroyedWhilePending_DestroysHandleAndIgnoresStaleReady() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        regularTabModel.setTabRemoverForTesting(mRegularTabRemover);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        Destroyable preparationHandle = mock(Destroyable.class);
        ArgumentCaptor<TabModelSelectorObserver> observerCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);
        Runnable readyCallback = startPendingCloseAll(/* allowUndo= */ false, preparationHandle);

        verify(mTabModelSelector).addObserver(observerCaptor.capture());
        observerCaptor.getValue().onDestroyed();

        verify(mTabModelSelector).removeObserver(observerCaptor.getValue());
        verify(preparationHandle, times(1)).destroy();

        // Stale readiness callback is a no-op; subsequent request is no longer deduplicated.
        readyCallback.run();
        verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());

        when(mArchivedTabsDelegate.prepareForCloseAll(any())).thenReturn(null);
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mArchivedTabsDelegate,
                /* allowUndo= */ false,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        verify(mRegularTabRemover, times(1)).closeTabs(any(), eq(false));
    }

    @Test
    public void testCloseAllTabs_RegularModelBecomesEmptyWhilePending_DestroysHandle() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        Destroyable preparationHandle = mock(Destroyable.class);
        Runnable readyCallback = startPendingCloseAll(/* allowUndo= */ false, preparationHandle);

        when(mTabModelSelector.getModel(false))
                .thenReturn(new StubTabModel(/* isIncognito= */ false, mProfile));
        readyCallback.run();

        verify(preparationHandle, times(1)).destroy();
        verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    @Test
    public void testCloseAllTabs_ProfileShutdownWhilePending_DestroysHandle() {
        MockTabModel regularTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        when(mTabModelSelector.getModel(false)).thenReturn(regularTabModel);
        Destroyable preparationHandle = mock(Destroyable.class);
        Runnable readyCallback = startPendingCloseAll(/* allowUndo= */ false, preparationHandle);

        when(mProfile.shutdownStarted()).thenReturn(true);
        readyCallback.run();

        verify(preparationHandle, times(1)).destroy();
        verify(mArchivedTabsDelegate, never()).unarchiveAndGetUndoRunnable();
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    private Runnable startPendingCloseAll(boolean allowUndo, Destroyable preparationHandle) {
        ArgumentCaptor<Runnable> readyCaptor = ArgumentCaptor.forClass(Runnable.class);
        when(mArchivedTabsDelegate.prepareForCloseAll(readyCaptor.capture()))
                .thenReturn(preparationHandle);
        CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                mTabModelSelector,
                mArchivedTabsDelegate,
                allowUndo,
                TabClosingSource.GRID_TAB_SWITCHER,
                /* closePinnedTabs= */ false);
        return readyCaptor.getValue();
    }
}
