// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
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

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link TabListMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public class ArchivedTabsDialogCoordinatorUnitTest {
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String TAB_GROUP_ID_STRING = TAB_GROUP_ID.toString();
    private static final int TAB1_ID = 456;

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
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private View mItemView1;
    @Mock private PaneManager mPaneManager;
    @Mock private TabSwitcherPaneBase mTabSwitcherPaneBase;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabGroupModelFilter mCurrentTabGroupModelFilter;

    private Activity mActivity;
    private ArchivedTabsDialogCoordinator mCoordinator;
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(1);
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<PaneManager> mPaneManagerSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        setUpMocks();
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRootView = spy(new FrameLayout(mActivity));
        mTabSwitcherView = new FrameLayout(mActivity);
        FrameLayout.LayoutParams layoutparams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_HORIZONTAL | Gravity.CENTER_VERTICAL);
        mTabSwitcherView.setLayoutParams(layoutparams);

        TabListRecyclerView recyclerView = new TabListRecyclerView(mActivity, null);
        recyclerView.setId(R.id.tab_list_recycler_view);
        mTabSwitcherView.addView(recyclerView);
        mPaneManagerSupplier.set(mPaneManager);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);
        mCurrentTabGroupModelFilterSupplier.set(mCurrentTabGroupModelFilter);

        mCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mActivity,
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
                        /* desktopWindowStateManager= */ null,
                        mEdgeToEdgeSupplier,
                        mTabGroupSyncService,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mCurrentTabGroupModelFilterSupplier);
        mCoordinator.setTabListEditorCoordinatorForTesting(mTabListEditorCoordinator);
        recyclerView = new TabListRecyclerView(mActivity, null);
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

        when(mArchivedTabModelOrchestrator.getTabModelSelector())
                .thenReturn(mArchivedTabModelSelector);
        when(mArchivedTabModelSelector.getModel(false)).thenReturn(mArchivedTabModel);
        when(mArchivedTabModelOrchestrator.getTabCountSupplier()).thenReturn(mTabCountSupplier);

        when(mTabListEditorCoordinator.getController()).thenReturn(mTabListEditorController);
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
        verify(mTabListEditorController).show(any(), eq(Collections.emptyList()), eq(null));
        verify(mTabListEditorController).setNavigationProvider(any());
        verify(mTabListEditorController, times(2)).setToolbarTitle("1 inactive tab");
        verify(mBackPressManager).addHandler(any(), eq(BackPressHandler.Type.ARCHIVED_TABS_DIALOG));

        mTabCountSupplier.set(2);
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS)
    public void testShowWithSyncedTabGroups() {
        List<String> tabGroupSyncIds = new ArrayList<>(List.of(TAB_GROUP_ID_STRING));
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroup.savedTabs = new ArrayList<>(List.of(savedTabGroupTab));
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {TAB_GROUP_ID_STRING});
        when(mTabGroupSyncService.getGroup(TAB_GROUP_ID_STRING)).thenReturn(savedTabGroup);

        mCoordinator.show(mOnTabSelectingListener);
        verify(mTabListEditorController).show(any(), eq(tabGroupSyncIds), eq(null));
    }

    @Test
    public void testAddRemoveTab() {
        mCoordinator.show(mOnTabSelectingListener);

        // First verify a tab exists as the base condition for showing.
        verify(mTabListEditorController, times(2)).setToolbarTitle("1 inactive tab");

        // Then add a second tab.
        mTabCountSupplier.set(2);
        verify(mTabListEditorController).setToolbarTitle("2 inactive tabs");

        // Then close both tabs.
        mTabCountSupplier.set(1);
        verify(mTabListEditorController, times(3)).setToolbarTitle("1 inactive tab");

        mTabCountSupplier.set(0);

        // Allow animations to finish.
        ShadowLooper.runUiThreadTasks();

        verify(mTabListEditorController).hide();
    }

    @Test
    public void testLifecycleObserverHidesDialog() {
        mCoordinator.show(mOnTabSelectingListener);
        mCoordinator.getTabListEditorLifecycleObserver().willHide();

        ShadowLooper.runUiThreadTasks();
        verify(mRootView).removeView(any());

        mCoordinator.getTabListEditorLifecycleObserver().didHide();
        verify(mTabListEditorController).setLifecycleObserver(null);
        verify(mBackPressManager).removeHandler(any());
    }

    @Test
    public void testDestroyHidesDialog() {
        when(mTabListEditorController.isVisible()).thenReturn(true);
        mCoordinator.show(mOnTabSelectingListener);
        mCoordinator.destroy();

        // Allow animations to finish.
        ShadowLooper.runUiThreadTasks();

        verify(mRootView, atLeastOnce()).removeView(any());
        verify(mTabListEditorController).setLifecycleObserver(null);
        verify(mBackPressManager).removeHandler(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgePadAdjuster() {
        EdgeToEdgePadAdjuster padAdjuster = mCoordinator.getEdgeToEdgePadAdjusterForTesting();
        assertNotNull("Pad adjuster should be created when feature enabled.", padAdjuster);

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(eq(padAdjuster));

        FrameLayout buttonContainer = mCoordinator.getCloseAllTabsButtonContainer();

        int bottomInset = 100;
        padAdjuster.overrideBottomInset(bottomInset);
        assertEquals(bottomInset, buttonContainer.getPaddingBottom());
        assertTrue("clipToPadding should not change.", buttonContainer.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals(0, buttonContainer.getPaddingBottom());
        assertTrue("clipToPadding should not change.", buttonContainer.getClipToPadding());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgePadAdjuster_FeatureDisabled() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        var padAdjuster = mCoordinator.getEdgeToEdgePadAdjusterForTesting();
        assertNull("Pad adjuster should be created when feature enabled.", padAdjuster);
    }

    @Test
    public void testGridCardOnClickProvider_restoreTabGroup() {
        SavedTabGroup savedTabGroupBefore = new SavedTabGroup();
        savedTabGroupBefore.syncId = TAB_GROUP_ID_STRING;

        SavedTabGroup savedTabGroupAfter = new SavedTabGroup();
        savedTabGroupAfter.syncId = TAB_GROUP_ID_STRING;
        savedTabGroupAfter.localId = new LocalTabGroupId(TAB_GROUP_ID);

        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPaneBase);
        when(mTabGroupSyncService.getGroup(TAB_GROUP_ID_STRING))
                .thenReturn(savedTabGroupBefore)
                .thenReturn(savedTabGroupBefore)
                .thenReturn(savedTabGroupAfter);
        when(mCurrentTabGroupModelFilter.getRootIdFromTabGroupId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        when(mTabListEditorController.isVisible()).thenReturn(true);

        // Show the dialog.
        mCoordinator.show(mOnTabSelectingListener);

        // Run the click listener.
        GridCardOnClickListenerProvider provider =
                mCoordinator.getGridCardOnClickListenerProviderForTesting();
        TabActionListener listener = provider.openTabGridDialog(TAB_GROUP_ID_STRING);
        listener.run(mItemView1, TAB_GROUP_ID_STRING, /* triggeringMotion= */ null);

        verify(mTabGroupUiActionHandler).openTabGroup(TAB_GROUP_ID_STRING);

        // Assert the dialog is hidden and destroyed.
        ShadowLooper.runUiThreadTasks();

        verify(mRootView, atLeastOnce()).removeView(any());
        verify(mTabListEditorController).setLifecycleObserver(null);
        verify(mBackPressManager).removeHandler(any());

        // Assert that the tab group has a request to open from GTS.
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(TAB1_ID);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE)
    public void testCloseAllTabsButtonBackgroundColor() {
        mCoordinator.show(mOnTabSelectingListener);
        FrameLayout buttonContainer = mCoordinator.getCloseAllTabsButtonContainer();
        assertEquals(
                SemanticColorUtils.getColorSurface(mActivity),
                ((ColorDrawable) buttonContainer.getBackground()).getColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE)
    public void testCloseAllTabsButtonBackgroundColorUpdate() {
        mCoordinator.show(mOnTabSelectingListener);
        FrameLayout buttonContainer = mCoordinator.getCloseAllTabsButtonContainer();
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity),
                ((ColorDrawable) buttonContainer.getBackground()).getColor());
    }
}
