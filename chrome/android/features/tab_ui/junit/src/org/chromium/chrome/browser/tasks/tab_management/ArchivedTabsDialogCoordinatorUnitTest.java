// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests for {@link TabListMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public class ArchivedTabsDialogCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Spy private ViewGroup mRootView;
    @Spy private ViewGroup mTabSwitcherView;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabModelSelectorBase mArchivedTabModelSelector;
    @Mock private TabModel mArchivedTabModel;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private TabListEditorController mTabListEditorController;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private BackPressManager mBackPressManager;
    @Mock private OnTabSelectingListener mOnTabSelectingListener;
    @Mock private TabArchiveSettings mTabArchiveSettings;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private RecyclerView mRecyclerView;

    private Context mContext;
    private ArchivedTabsDialogCoordinator mCoordinator;
    private ObservableSupplierImpl<Integer> mTabCountSupplier = new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        setUpMocks();
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mRootView = spy(new FrameLayout(mContext));
        mTabSwitcherView = new FrameLayout(mContext);
        FrameLayout.LayoutParams layoutparams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_HORIZONTAL | Gravity.CENTER_VERTICAL);
        mTabSwitcherView.setLayoutParams(layoutparams);

        TabListRecyclerView recyclerView = new TabListRecyclerView(mContext, null);
        recyclerView.setId(R.id.tab_list_recycler_view);
        mTabSwitcherView.addView(recyclerView);

        mCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mContext,
                        mArchivedTabModelOrchestrator,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mTabSwitcherView,
                        mSnackbarManager,
                        mRegularTabCreator,
                        mBackPressManager,
                        mTabArchiveSettings,
                        mModalDialogManager,
                        /* desktopWindowStateProvider= */ null);
        mCoordinator.setTabListEditorCoordinatorForTesting(mTabListEditorCoordinator);
        recyclerView = new TabListRecyclerView(mContext, null);
        recyclerView.setId(R.id.tab_list_recycler_view);
        ((ViewGroup) mCoordinator.getViewForTesting().findViewById(R.id.tab_list_editor_container))
                .addView(recyclerView);
    }

    private void setUpMocks() {
        // Run posted tasks immediately.
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        doReturn(mArchivedTabModelSelector)
                .when(mArchivedTabModelOrchestrator)
                .getTabModelSelector();
        doReturn(mArchivedTabModel).when(mArchivedTabModelSelector).getModel(false);
        doReturn(mTabCountSupplier).when(mArchivedTabModel).getTabCountSupplier();

        doReturn(mTabListEditorController).when(mTabListEditorCoordinator).getController();
        doAnswer(
                        invocationOnMock -> {
                            mCoordinator.getTabListEditorLifecycleObserver().willHide();
                            mCoordinator.getTabListEditorLifecycleObserver().didHide();
                            return null;
                        })
                .when(mTabListEditorController)
                .hide();
    }

    @Test
    public void testShow() {
        mCoordinator.show(mOnTabSelectingListener);
        verify(mRootView).addView(any());
        verify(mTabListEditorController).setNavigationProvider(any());
        verify(mTabListEditorController).setToolbarTitle("0 inactive tabs");
        verify(mBackPressManager).addHandler(any(), eq(BackPressHandler.Type.ARCHIVED_TABS_DIALOG));

        doReturn(1).when(mArchivedTabModel).getCount();
        mCoordinator.updateTitle();
        verify(mTabListEditorController).setToolbarTitle("1 inactive tab");

        doReturn(2).when(mArchivedTabModel).getCount();
        mCoordinator.updateTitle();
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");
    }

    @Test
    public void testAddRemoveTab() {
        mCoordinator.show(mOnTabSelectingListener);

        // First add a tab
        doReturn(1).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(1);
        verify(mTabListEditorController).setToolbarTitle("1 inactive tab");

        // Then a second
        doReturn(2).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(2);
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");

        // Then close bloth
        doReturn(1).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(1);
        verify(mTabListEditorController, times(2)).setToolbarTitle("1 inactive tab");

        doReturn(0).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(0);

        // Allow animations to finish.
        ShadowLooper.runUiThreadTasks();

        verify(mTabListEditorController).hide();
    }

    @Test
    public void testLifecycleObserverHidesDialog() {
        mCoordinator.show(mOnTabSelectingListener);
        mCoordinator.getTabListEditorLifecycleObserver().willHide();
        verify(mRootView).removeView(any());

        mCoordinator.getTabListEditorLifecycleObserver().didHide();
        verify(mTabListEditorController).setLifecycleObserver(null);
        verify(mBackPressManager).removeHandler(any());
    }

    @Test
    public void testDestroyHidesDialog() {
        doReturn(true).when(mTabListEditorController).isVisible();
        mCoordinator.show(mOnTabSelectingListener);
        mCoordinator.destroy();

        // Allow animations to finish.
        ShadowLooper.runUiThreadTasks();

        verify(mRootView).removeView(any());
        verify(mTabListEditorController).setLifecycleObserver(null);
        verify(mBackPressManager).removeHandler(any());
    }
}
