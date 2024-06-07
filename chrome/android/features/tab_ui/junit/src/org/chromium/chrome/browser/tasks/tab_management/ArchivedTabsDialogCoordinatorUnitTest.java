// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link TabListMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabsDialogCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabModelSelectorBase mArchivedTabModelSelector;
    @Mock private TabModel mArchivedTabModel;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private TabListEditorController mTabListEditorController;
    @Spy private ViewGroup mRootView;

    private Context mContext;
    private ArchivedTabsDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        setUpMocks();
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mRootView = spy(new FrameLayout(mContext));
        mCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mContext,
                        mArchivedTabModelOrchestrator,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mSnackbarManager);
        mCoordinator.setTabListEditorCoordinatorForTesting(mTabListEditorCoordinator);
    }

    private void setUpMocks() {
        doReturn(mArchivedTabModelSelector)
                .when(mArchivedTabModelOrchestrator)
                .getTabModelSelector();
        doReturn(mArchivedTabModel).when(mArchivedTabModelSelector).getModel(false);

        doReturn(mTabListEditorController).when(mTabListEditorCoordinator).getController();
    }

    @Test
    public void testShow() {
        mCoordinator.show();
        verify(mRootView).addView(any());
        verify(mTabListEditorController).setNavigationProvider(any());
        verify(mTabListEditorController).setToolbarTitle("0 inactive tabs");

        doReturn(1).when(mArchivedTabModel).getCount();
        mCoordinator.updateTitle();
        verify(mTabListEditorController).setToolbarTitle("1 inactive tab");

        doReturn(2).when(mArchivedTabModel).getCount();
        mCoordinator.updateTitle();
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");
    }

    @Test
    public void testAddRemoveTab() {
        mCoordinator.show();
        verify(mArchivedTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        // First add a tab
        doReturn(1).when(mArchivedTabModel).getCount();
        observer.willAddTab(null, 0);
        verify(mTabListEditorController).setToolbarTitle("1 inactive tab");

        // Then a second
        doReturn(2).when(mArchivedTabModel).getCount();
        observer.willAddTab(null, 0);
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");

        // Then close bloth
        doReturn(1).when(mArchivedTabModel).getCount();
        observer.willCloseTab(null, true);
        verify(mTabListEditorController, times(2)).setToolbarTitle("1 inactive tab");

        doReturn(0).when(mArchivedTabModel).getCount();
        observer.willCloseTab(null, true);
        verify(mRootView).removeView(any());
    }
}
