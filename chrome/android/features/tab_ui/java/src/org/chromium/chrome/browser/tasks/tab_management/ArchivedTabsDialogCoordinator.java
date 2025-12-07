// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.core.util.Function;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.SavedTabGroupUndoBarController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

@NullMarked
public class ArchivedTabsDialogCoordinator implements SnackbarManager.SnackbarManageable {

    private static final int ANIM_DURATION_MS = 250;
    public static final String COMPONENT_NAME = "ArchivedTabsDialog";

    /** Interface exposing functionality to the menu items for the archived tabs dialog */
    public interface ArchiveDelegate {
        /** Restore all tabs from the archived tab model. */
        void restoreAllArchivedTabs();

        /** Open the archive settings page. */
        void openArchiveSettings();

        /** Start tab selection process. */
        void startTabSelection();

        /** Restore the given list of tabs and tab groups. */
        void restoreArchivedTabs(List<Tab> tabs, List<String> tabGroupSyncIds);

        /** Close the given list of tabs and tab groups. */
        void closeArchivedTabs(List<Tab> tabs, List<String> tabGroupSyncIds);
    }

    private final ArchiveDelegate mArchiveDelegate =
            new ArchiveDelegate() {
                @Override
                public void restoreAllArchivedTabs() {
                    List<Tab> tabs = TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel);
                    int tabCount = tabs.size();
                    List<String> archivedTabGroupSyncIds = getArchivedTabGroupSyncIds();
                    int tabGroupTabCount = getSyncedTabGroupTabsCount(archivedTabGroupSyncIds);
                    ArchivedTabsDialogCoordinator.this.restoreArchivedTabs(
                            tabs, archivedTabGroupSyncIds);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.RestoreAllArchivedTabsMenuItem.TabCount", tabCount);
                    RecordUserAction.record("Tabs.RestoreAllArchivedTabsMenuItem");
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.RestoreAllArchivedTabsMenuItem.TabGroupCount",
                            archivedTabGroupSyncIds.size());
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.RestoreAllArchivedTabsMenuItem.TabGroupTabCount",
                            tabGroupTabCount);
                }

                @Override
                public void openArchiveSettings() {
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mActivity, TabArchiveSettingsFragment.class);
                    RecordUserAction.record("Tabs.OpenArchivedTabsSettingsMenuItem");
                }

                @Override
                public void startTabSelection() {
                    moveToState(TabActionState.SELECTABLE);
                    RecordUserAction.record("Tabs.SelectArchivedTabsMenuItem");
                }

                @Override
                public void restoreArchivedTabs(List<Tab> tabs, List<String> tabGroupSyncIds) {
                    int tabCount = tabs.size();
                    ArchivedTabsDialogCoordinator.this.restoreArchivedTabs(tabs, tabGroupSyncIds);
                    moveToState(TabActionState.CLOSABLE);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.RestoreArchivedTabsMenuItem.TabCount", tabCount);
                    RecordUserAction.record("Tabs.RestoreArchivedTabsMenuItem");
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.RestoreArchivedTabsMenuItem.TabGroupCount",
                            tabGroupSyncIds.size());
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.RestoreArchivedTabsMenuItem.TabGroupTabCount",
                            getSyncedTabGroupTabsCount(tabGroupSyncIds));
                }

                @Override
                public void closeArchivedTabs(List<Tab> tabs, List<String> tabGroupSyncIds) {
                    mArchivedTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTabs(tabs).build(),
                                    /* allowDialog= */ false);
                    closeArchivedTabGroups(tabGroupSyncIds);
                    mUndoBarController.queueUndoBar(tabs, tabGroupSyncIds);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.CloseArchivedTabsMenuItem.TabCount", tabs.size());
                    RecordUserAction.record("Tabs.CloseArchivedTabsMenuItem");
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.CloseArchivedTabsMenuItem.TabGroupCount",
                            tabGroupSyncIds.size());
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.CloseArchivedTabsMenuItem.TabGroupTabCount",
                            getSyncedTabGroupTabsCount(tabGroupSyncIds));
                }
            };

    private final NavigationProvider mNavigationProvider =
            new NavigationProvider() {
                @Override
                public void goBack() {
                    if (mTabActionState == TabActionState.CLOSABLE) {
                        hide(
                                ANIM_DURATION_MS,
                                /* animationFinishCallback= */ CallbackUtils.emptyRunnable());
                    } else {
                        moveToState(TabActionState.CLOSABLE);
                    }
                }
            };

    /**
     * Observes the tab count in the archived tab model to (1) update the title and (2) hide the
     * dialog when no archived tabs remain.
     */
    private final Callback<Integer> mTabCountObserver =
            (tabCount) -> {
                if (tabCount == 0 && !ArchivedTabsDialogCoordinator.this.mIsOpeningLastItem) {
                    // Post task to allow the last tab to be unregistered.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> hide(ANIM_DURATION_MS, CallbackUtils.emptyRunnable()));
                    return;
                }
                updateTitle();
            };

    /** Used to override the default tab click behavior to restore/open the tab. */
    private final GridCardOnClickListenerProvider mGridCardOnClickListenerProvider =
            new GridCardOnClickListenerProvider() {
                @Nullable
                @Override
                public TabActionListener openTabGridDialog(Tab tab) {
                    return null;
                }

                @Nullable
                @Override
                public TabActionListener openTabGridDialog(String syncId) {
                    return new TabActionListener() {
                        @Override
                        public void run(
                                View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                            // Intentional no-op.
                        }

                        @Override
                        public void run(
                                View view,
                                String syncId,
                                @Nullable MotionEventInfo triggeringMotion) {
                            assumeNonNull(mTabGroupSyncService);
                            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncId);
                            assumeNonNull(savedTabGroup);
                            mIsOpeningLastItem =
                                    getArchivedTabCount() == savedTabGroup.savedTabs.size();

                            TabSwitcherPaneBase tabSwitcherPaneBase =
                                    (TabSwitcherPaneBase)
                                            mPaneManagerSupplier.get().getDefaultPane();
                            assumeNonNull(tabSwitcherPaneBase);
                            Callback<Integer> requestOpenTabGroupDialog =
                                    (rootId) -> {
                                        hide(
                                                ANIM_DURATION_MS,
                                                () -> {
                                                    tabSwitcherPaneBase.requestOpenTabGroupDialog(
                                                            rootId);
                                                });
                                    };
                            // Archive status is reset through any tab group open action in
                            // LocalTabGroupMutationHelper#createNewTabGroup().
                            TabSwitcherUtils.openTabGroupDialog(
                                    syncId,
                                    mTabGroupSyncService,
                                    mTabGroupUiActionHandlerSupplier.get(),
                                    assumeNonNull(mCurrentTabGroupModelFilterSupplier.get()),
                                    requestOpenTabGroupDialog);
                            RecordUserAction.record("TabGroups.RestoreSingleTabGroup");
                            RecordHistogram.recordCount1000Histogram(
                                    "TabGroups.RestoreSingleTabGroup.TabGroupTabCount",
                                    savedTabGroup.savedTabs.size());
                        }
                    };
                }

                @Override
                public void onTabSelecting(int tabId, boolean fromActionButton) {
                    mIsOpeningLastItem = getArchivedTabCount() == 1;
                    Tab tab = mArchivedTabModel.getTabById(tabId);
                    assumeNonNull(tab);
                    mArchivedTabModelOrchestrator
                            .getTabArchiver()
                            .unarchiveAndRestoreTabs(
                                    mRegularTabCreator,
                                    Arrays.asList(tab),
                                    /* updateTimestamp= */ true,
                                    /* areTabsBeingOpened= */ true);

                    hide(
                            ANIM_DURATION_MS,
                            () -> {
                                // Post task to allow the tab to be unregistered.
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> {
                                            if (mOnTabSelectingListener != null) {
                                                mOnTabSelectingListener.onTabSelecting(tab.getId());
                                            }
                                        });
                                RecordUserAction.record("Tabs.RestoreSingleTab");
                            });
                }
            };

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    updateIphPropertyModel();
                }
            };

    private final TabGroupSyncService.Observer mTabGroupSyncObserver =
            new TabGroupSyncService.Observer() {
                @Override
                public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {
                    if (group.archivalTimeMs != null) {
                        refreshArchivedTabList();
                    }
                }

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localId, @TriggerSource int source) {
                    refreshArchivedTabList();

                    if (mTabActionState == TabActionState.SELECTABLE) {
                        moveToState(TabActionState.CLOSABLE);
                    }
                }

                @Override
                public void onTabGroupRemoved(String syncId, @TriggerSource int source) {
                    refreshArchivedTabList();

                    if (mTabActionState == TabActionState.SELECTABLE) {
                        moveToState(TabActionState.CLOSABLE);
                    }
                }
            };

    /**
     * Observes the TabListEditor lifecycle to remove the view and hide the dialog. This is useful
     * for when (1) the TabListEditor is expecting the embedding view to be removed from the
     * hierarchy prior to hide completion. (2) If the TabListEditor hides itself outside of the
     * dialog control flow, we want to know about it in order to hide the embedding UI.
     */
    private final TabListEditorCoordinator.LifecycleObserver mTabListEditorLifecycleObserver =
            new TabListEditorCoordinator.LifecycleObserver() {
                @Override
                public void willHide() {
                    mDialogRecyclerView.removeOnScrollListener(mRecyclerScrollListener);
                    mSnackbarManager.popParentViewFromOverrideStack(mSnackbarOverrideToken);
                    // In case we were hidden by TabListEditor in some other case, force the
                    // animation to finish.
                    animateOut(
                            /* duration= */ 0,
                            /* animationFinishCallback= */ CallbackUtils.emptyRunnable());
                }

                @Override
                public void didHide() {
                    ArchivedTabsDialogCoordinator.this.hideInternal();
                }
            };

    private final RecyclerView.OnScrollListener mRecyclerScrollListener =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(RecyclerView recyclerView, int newState) {}

                @Override
                public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                    mShadowView.setVisibility(
                            recyclerView.canScrollVertically(1) ? View.VISIBLE : View.GONE);
                }
            };

    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver =
            new TabListItemSizeChangedObserver() {
                @Override
                public void onSizeChanged(int spanCount, Size cardSize) {
                    if (mIphMessagePropertyModel == null) return;
                    mIphMessagePropertyModel.set(
                            ResizableMessageCardViewProperties.WIDTH,
                            spanCount == 4 ? cardSize.getWidth() * 2 : MATCH_PARENT);
                }
            };

    private final Activity mActivity;
    private final ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final TabModel mArchivedTabModel;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabContentManager mTabContentManager;
    private final @TabListMode int mMode;
    private final ViewGroup mRootView;
    private final SnackbarManager mSnackbarManager;
    private final TabCreator mRegularTabCreator;
    private final BackPressManager mBackPressManager;
    private final TabArchiveSettings mTabArchiveSettings;
    private final ModalDialogManager mModalDialogManager;
    private final SavedTabGroupUndoBarController mUndoBarController;
    private final ActionConfirmationDialog mActionConfirmationDialog;
    private final ViewGroup mDialogView;
    private final ViewGroup mTabSwitcherView;
    private final FadingShadowView mShadowView;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    private EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;
    private TabListRecyclerView mDialogRecyclerView;
    private WeakReference<TabListRecyclerView> mTabSwitcherRecyclerView;
    private @TabActionState int mTabActionState = TabActionState.CLOSABLE;
    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;
    private @Nullable OnTabSelectingListener mOnTabSelectingListener;
    private @Nullable PropertyModel mIphMessagePropertyModel;
    private int mSnackbarOverrideToken;
    private boolean mIsOpeningLastItem;
    private boolean mIsShowing;

    /**
     * @param activity The android activity.
     * @param archivedTabModelOrchestrator The TabModelOrchestrator for archived tabs.
     * @param browserControlsStateProvider Used as a dependency to TabListEditorCoordiantor.
     * @param tabContentManager Used as a dependency to TabListEditorCoordiantor.
     * @param mode Used as a dependency to TabListEditorCoordiantor.
     * @param rootView Used as a dependency to TabListEditorCoordiantor.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param regularTabCreator Handles the creation of regular tabs.
     * @param backPressManager Manages the different back press handlers throughout the app.
     * @param tabArchiveSettings The settings manager for tab archive.
     * @param modalDialogManager Used for managing the modal dialogs.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param edgeToEdgeSupplier Supplier for the {@link EdgeToEdgeController}.
     * @param tabGroupSyncService The {@link TabGroupSyncService} used for tab group sync.
     * @param paneManagerSupplier Used to switch and communicate with other panes.
     * @param tabGroupUiActionHandlerSupplier Used to open hidden tab groups.
     * @param currentTabGroupModelFilterSupplier The supplier of the current {@link
     *     TabGroupModelFilter}.
     */
    public ArchivedTabsDialogCoordinator(
            Activity activity,
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            BrowserControlsStateProvider browserControlsStateProvider,
            TabContentManager tabContentManager,
            @TabListMode int mode,
            ViewGroup rootView,
            ViewGroup tabSwitcherView,
            SnackbarManager snackbarManager,
            TabCreator regularTabCreator,
            BackPressManager backPressManager,
            TabArchiveSettings tabArchiveSettings,
            ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier) {
        mActivity = activity;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mMode = mode;
        mRootView = rootView;
        mSnackbarManager = snackbarManager;
        mRegularTabCreator = regularTabCreator;
        mBackPressManager = backPressManager;
        mTabArchiveSettings = tabArchiveSettings;
        mModalDialogManager = modalDialogManager;
        mDesktopWindowStateManager = desktopWindowStateManager;

        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        TabModelSelectorBase tabModelSelector = mArchivedTabModelOrchestrator.getTabModelSelector();
        assumeNonNull(tabModelSelector);
        mArchivedTabModel = tabModelSelector.getModel(/* incognito= */ false);
        mUndoBarController =
                new SavedTabGroupUndoBarController(
                        mActivity,
                        tabModelSelector,
                        /* snackbarManageable= */ this,
                        tabGroupSyncService);
        mTabSwitcherView = tabSwitcherView;

        // Inflate the dialog view and hook it up
        mDialogView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.archived_tabs_dialog, mRootView, false);
        mDialogView
                .findViewById(R.id.close_all_tabs_button)
                .setOnClickListener(this::onCloseAllInactiveTabsButtonClicked);

        // Initialize the shadow for the "Close all inactive tabs" container.
        mShadowView = mDialogView.findViewById(R.id.close_all_tabs_button_container_shadow);
        mShadowView.init(
                mActivity.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_BOTTOM);

        // Initialize the confirmation dialog for when the last archived tab is removed.
        mActionConfirmationDialog = new ActionConfirmationDialog(mActivity, mModalDialogManager);

        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mEdgeToEdgePadAdjuster =
                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                        getCloseAllTabsButtonContainer(), mEdgeToEdgeSupplier);

        mTabGroupSyncService = tabGroupSyncService;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;

        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
    }

    /** Hides the dialog. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mDestroyChecker.checkNotDestroyed();
        mDestroyChecker.destroy();

        if (mTabListEditorCoordinator != null) {
            mRootView.removeView(mDialogView);
            mTabListEditorCoordinator.removeTabListItemSizeChangedObserver(
                    mTabListItemSizeChangedObserver);
            mTabListEditorCoordinator.getController().hide();
            tearDownTabListEditorCoordinator();
        }

        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }

        if (mDialogRecyclerView != null) {
            mDialogRecyclerView.removeOnScrollListener(mRecyclerScrollListener);
        }

        if (mOnTabSelectingListener != null) {
            mOnTabSelectingListener = null;
        }

        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }

        mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
    }

    private void tearDownTabListEditorCoordinator() {
        assumeNonNull(mTabListEditorCoordinator);
        mTabListEditorCoordinator.destroy();
        mTabListEditorCoordinator = null;
    }

    /**
     * Shows the dialog.
     *
     * @param onTabSelectingListener Allows a tab to be selected in the main tab switcher.
     */
    public void show(OnTabSelectingListener onTabSelectingListener) {
        if (mIsShowing) return;
        showInternal(onTabSelectingListener);
    }

    @Initializer
    private void showInternal(OnTabSelectingListener onTabSelectingListener) {
        mIsShowing = true;
        TabListRecyclerView tabListRecyclerView =
                mTabSwitcherView.findViewById(R.id.tab_list_recycler_view);
        assumeNonNull(tabListRecyclerView);
        mTabSwitcherRecyclerView = new WeakReference<>(tabListRecyclerView);
        tabListRecyclerView.setBlockTouchInput(true);

        boolean tabListFirstShown = false;
        if (mTabListEditorCoordinator == null) {
            tabListFirstShown = true;
            createTabListEditorCoordinator();
        }

        mOnTabSelectingListener = onTabSelectingListener;
        mArchivedTabModelOrchestrator.getTabCountSupplier().addObserver(mTabCountObserver);

        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.setLifecycleObserver(mTabListEditorLifecycleObserver);
        controller.show(
                TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel),
                getArchivedTabGroupSyncIds(),
                /* recyclerViewPosition= */ null);
        controller.setNavigationProvider(mNavigationProvider);
        mTabListEditorCoordinator.overrideContentDescriptions(
                R.string.accessibility_archived_tabs_dialog,
                R.string.accessibility_archived_tabs_dialog_back_button);

        mDialogRecyclerView = mDialogView.findViewById(R.id.tab_list_recycler_view);
        mDialogRecyclerView.addOnScrollListener(mRecyclerScrollListener);
        mShadowView.setVisibility(
                mDialogRecyclerView.canScrollVertically(1) ? View.VISIBLE : View.GONE);

        // Register the dialog to handle back press events.
        mBackPressManager.addHandler(controller, BackPressHandler.Type.ARCHIVED_TABS_DIALOG);

        FrameLayout snackbarContainer = mDialogView.findViewById(R.id.snackbar_container);
        mSnackbarOverrideToken = mSnackbarManager.pushParentViewToOverrideStack(snackbarContainer);
        // View is obscured by the TabListEditorCoordinator, so it needs to be brought to the front.
        mDialogView.findViewById(R.id.close_all_tabs_button_container).bringToFront();
        snackbarContainer.bringToFront();

        // Add the IPH to the TabListEditor.
        if (mTabArchiveSettings.shouldShowDialogIph()) {
            if (tabListFirstShown) {
                mTabListEditorCoordinator.registerItemType(
                        UiType.ARCHIVED_TABS_IPH_MESSAGE,
                        new LayoutViewBuilder<>(R.layout.resizable_tab_grid_message_card_item),
                        ResizableMessageCardViewBinder::bind);
            }
            mIphMessagePropertyModel =
                    ArchivedTabsIphMessageCardViewModel.create(
                            mActivity, this::onIphReviewClicked, this::onIphDismissClicked);
            updateIphPropertyModel();
            mTabListEditorCoordinator.addSpecialListItem(
                    0, UiType.ARCHIVED_TABS_IPH_MESSAGE, mIphMessagePropertyModel);
            mTabListEditorCoordinator.addTabListItemSizeChangedObserver(
                    mTabListItemSizeChangedObserver);
            RecordUserAction.record("Tabs.ArchivedTabsDialogIphShown");
        }
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);

        moveToState(TabActionState.CLOSABLE);
        animateIn(ANIM_DURATION_MS);
    }

    private void animateIn(int duration) {
        mDialogView.setVisibility(View.INVISIBLE);
        mRootView.addView(mDialogView);

        mDialogView.post(
                () -> {
                    int dialogViewWidth = mDialogView.getWidth();
                    int translationFactor =
                            LocalizationUtils.isLayoutRtl() ? -dialogViewWidth : dialogViewWidth;
                    mDialogView.setVisibility(View.VISIBLE);
                    mDialogView.setTranslationX(translationFactor);

                    AnimatorSet animatorSet = new AnimatorSet();
                    animatorSet.setDuration(duration);
                    animatorSet.playTogether(getAnimateInAnimators());
                    animatorSet.start();

                    RecordUserAction.record("Tabs.ArchivedTabsDialogShown");
                });
    }

    private List<Animator> getAnimateInAnimators() {
        int tabSwitcherViewWidth = mTabSwitcherView.getWidth();
        int translationFactor =
                LocalizationUtils.isLayoutRtl() ? tabSwitcherViewWidth : -tabSwitcherViewWidth;
        List<Animator> animators = new ArrayList<>(2);
        ObjectAnimator animator = ObjectAnimator.ofFloat(mDialogView, View.TRANSLATION_X, 0f);
        animator.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        animators.add(animator);

        animator = ObjectAnimator.ofFloat(mTabSwitcherView, View.TRANSLATION_X, translationFactor);
        animator.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        animators.add(animator);

        return animators;
    }

    private void animateOut(int duration, Runnable animationFinishCallback) {
        mDialogRecyclerView.setBlockTouchInput(true);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.setDuration(duration);
        animatorSet.playTogether(getAnimateOutAnimators());
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mRootView.removeView(mDialogView);
                        animationFinishCallback.run();
                        mDialogRecyclerView.setBlockTouchInput(false);
                        animation.removeAllListeners();
                    }
                });
        animatorSet.start();
    }

    private List<Animator> getAnimateOutAnimators() {
        int dialogViewWidth = mDialogView.getWidth();
        int translationFactor =
                LocalizationUtils.isLayoutRtl() ? -dialogViewWidth : dialogViewWidth;
        List<Animator> animators = new ArrayList<>(2);
        ObjectAnimator animator =
                ObjectAnimator.ofFloat(mDialogView, View.TRANSLATION_X, translationFactor);
        animator.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        animators.add(animator);

        animator = ObjectAnimator.ofFloat(mTabSwitcherView, View.TRANSLATION_X, 0f);
        animator.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        animators.add(animator);
        return animators;
    }

    /** Hides the dialog. */
    public void hide(int animationDuration, Runnable animationFinishCallback) {
        animateOut(
                animationDuration,
                () -> {
                    // The mTabListEditorCoordinator may be teared down and destroyed after
                    // the animation finished.
                    if (mTabListEditorCoordinator != null) {
                        mTabListEditorCoordinator.removeTabListItemSizeChangedObserver(
                                mTabListItemSizeChangedObserver);
                        TabListEditorController controller =
                                mTabListEditorCoordinator.getController();
                        controller.hide();
                    }
                    animationFinishCallback.run();
                });
    }

    void hideInternal() {
        assumeNonNull(mTabListEditorCoordinator);
        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.setLifecycleObserver(null);
        mBackPressManager.removeHandler(mTabListEditorCoordinator.getController());
        mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
        mArchivedTabModelOrchestrator.getTabCountSupplier().removeObserver(mTabCountObserver);
        mSnackbarOverrideToken = TokenHolder.INVALID_TOKEN;
        mIsShowing = false;
        TabListRecyclerView recyclerView = mTabSwitcherRecyclerView.get();
        if (recyclerView != null) {
            recyclerView.setBlockTouchInput(false);
        }
        mTabSwitcherRecyclerView.clear();
    }

    void moveToState(@TabActionState int tabActionState) {
        mTabActionState = tabActionState;
        if (mTabListEditorCoordinator == null) return;
        mTabListEditorCoordinator.getController().setTabActionState(mTabActionState);
        updateTitle();

        List<TabListEditorAction> actions = new ArrayList<>();
        if (mTabActionState == TabActionState.CLOSABLE) {
            actions.add(TabListEditorRestoreAllArchivedTabsAction.createAction(mArchiveDelegate));
            actions.add(TabListEditorSelectArchivedTabsAction.createAction(mArchiveDelegate));
            actions.add(TabListEditorArchiveSettingsAction.createAction(mArchiveDelegate));
        } else if (mTabActionState == TabActionState.SELECTABLE) {
            actions.add(TabListEditorRestoreArchivedTabsAction.createAction(mArchiveDelegate));
            actions.add(TabListEditorCloseArchivedTabsAction.createAction(mArchiveDelegate));
        }

        mTabListEditorCoordinator.getController().configureToolbarWithMenuItems(actions);
    }

    @VisibleForTesting
    void updateTitle() {
        if (mTabListEditorCoordinator == null) return;
        int numInactiveTabs = getArchivedTabCount();
        String title =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.archived_tabs_dialog_title,
                                numInactiveTabs,
                                numInactiveTabs);
        mTabListEditorCoordinator.getController().setToolbarTitle(title);
    }

    @EnsuresNonNull("mTabListEditorCoordinator")
    private void createTabListEditorCoordinator() {
        mTabListEditorCoordinator =
                new TabListEditorCoordinator(
                        mActivity,
                        mRootView,
                        /* parentView= */ mDialogView.findViewById(R.id.tab_list_editor_container),
                        mBrowserControlsStateProvider,
                        assumeNonNull(mArchivedTabModelOrchestrator.getTabModelSelector())
                                .getTabGroupModelFilterProvider()
                                .getCurrentTabGroupModelFilterSupplier(),
                        mTabContentManager,
                        /* clientTabListRecyclerViewPositionSetter= */ CallbackUtils
                                .emptyCallback(),
                        mMode,
                        /* displayGroups= */ true,
                        mSnackbarManager,
                        /* bottomSheetController= */ null,
                        TabProperties.TabActionState.CLOSABLE,
                        mGridCardOnClickListenerProvider,
                        mModalDialogManager,
                        mDesktopWindowStateManager,
                        /* edgeToEdgeSupplier= */ null,
                        CreationMode.FULL_SCREEN,
                        mUndoBarController,
                        COMPONENT_NAME,
                        TabListEditorCoordinator.UNLIMITED_SELECTION);
    }

    @VisibleForTesting
    void onCloseAllInactiveTabsButtonClicked(View view) {
        int tabCount = mArchivedTabModel.getCount();
        List<String> archivedTabGroupSyncIds = getArchivedTabGroupSyncIds();
        int tabGroupTabsCount = getSyncedTabGroupTabsCount(archivedTabGroupSyncIds);
        showCloseAllArchivedTabsConfirmation(
                tabCount + tabGroupTabsCount,
                archivedTabGroupSyncIds,
                () -> {
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.CloseAllArchivedTabs.TabCount", tabCount);
                    RecordUserAction.record("Tabs.CloseAllArchivedTabsMenuItem");
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.CloseAllArchivedTabGroups.TabGroupCount",
                            archivedTabGroupSyncIds.size());
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.CloseAllArchivedTabGroups.TabGroupTabCount",
                            tabGroupTabsCount);
                });
    }

    /**
     * Shows a confirmation dialog when the close operation cannot be undone.
     *
     * @param tabCount Total number of tabs to be closed.
     * @param archivedTabGroupSyncIds The syncIds representing {@link SavedTabGroup}s to be closed.
     * @param onConfirmRunnable A runnable which is run if the dialog is confirmed.
     */
    private void showCloseAllArchivedTabsConfirmation(
            int tabCount, List<String> archivedTabGroupSyncIds, Runnable onConfirmRunnable) {
        Function<Resources, String> titleResolver =
                (res) -> {
                    return res.getQuantityString(
                            R.plurals.archive_dialog_close_all_inactive_tabs_confirmation_title,
                            tabCount,
                            tabCount);
                };
        Function<Resources, String> descriptionResolver =
                (res) -> {
                    return res.getString(
                            R.string
                                    .archive_dialog_close_all_inactive_tabs_confirmation_description);
                };
        mActionConfirmationDialog.show(
                titleResolver,
                descriptionResolver,
                R.string.archive_dialog_close_all_inactive_tabs_confirmation,
                R.string.cancel,
                /* supportStopShowing= */ false,
                (dismissHandler, buttonClickResult, stopShowing) -> {
                    if (buttonClickResult == ButtonClickResult.POSITIVE) {
                        mArchivedTabModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTabs(
                                                        TabModelUtils.convertTabListToListOfTabs(
                                                                mArchivedTabModel))
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false);
                        closeArchivedTabGroups(archivedTabGroupSyncIds);
                        onConfirmRunnable.run();
                    }
                    return DialogDismissType.DISMISS_IMMEDIATELY;
                });
    }

    private int getArchivedTabCount() {
        return mArchivedTabModelOrchestrator.getTabCountSupplier().get();
    }

    private void restoreArchivedTabs(List<Tab> tabs, List<String> tabGroupSyncIds) {
        mArchivedTabModelOrchestrator
                .getTabArchiver()
                .unarchiveAndRestoreTabs(
                        mRegularTabCreator,
                        tabs,
                        /* updateTimestamp= */ true,
                        /* areTabsBeingOpened= */ false);
        for (String syncId : tabGroupSyncIds) {
            mTabGroupUiActionHandlerSupplier.get().openTabGroup(syncId);
            if (mTabListEditorCoordinator != null) {
                mTabListEditorCoordinator.removeListItem(
                        UiType.TAB_GROUP,
                        TabListEditorItemSelectionId.createTabGroupSyncId(syncId));
            }
        }
    }

    private void onIphReviewClicked() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mActivity, TabArchiveSettingsFragment.class);
        RecordUserAction.record("Tabs.ArchivedTabsDialogIphClicked");
    }

    private void onIphDismissClicked(@MessageType int messageType) {
        mTabArchiveSettings.markDialogIphDismissed();
        if (mTabListEditorCoordinator != null) {
            mTabListEditorCoordinator.removeSpecialListItem(
                    UiType.ARCHIVED_TABS_IPH_MESSAGE, MessageType.ARCHIVED_TABS_IPH_MESSAGE);
        }

        RecordUserAction.record("Tabs.ArchivedTabsDialogIphDismissed");
    }

    private void updateIphPropertyModel() {
        if (mIphMessagePropertyModel == null) return;
        mIphMessagePropertyModel.set(
                MessageCardViewProperties.DESCRIPTION_TEXT,
                getIphDescription(mActivity, mTabArchiveSettings));
    }

    private void refreshArchivedTabList() {
        if (mTabListEditorCoordinator == null) return;
        mTabListEditorCoordinator.resetWithListOfTabs(
                TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel),
                getArchivedTabGroupSyncIds(),
                /* quickMode= */ false);
        if (mTabArchiveSettings.shouldShowDialogIph()) {
            assumeNonNull(mIphMessagePropertyModel);
            mTabListEditorCoordinator.addSpecialListItem(
                    0, UiType.ARCHIVED_TABS_IPH_MESSAGE, mIphMessagePropertyModel);
        }
    }

    @VisibleForTesting
    public static CharSequence getIphDescription(
            Context context, TabArchiveSettings tabArchiveSettings) {
        int archiveTimeDeltaDays = tabArchiveSettings.getArchiveTimeDeltaDays();
        int autoDeleteTimeDeltaMonths = tabArchiveSettings.getAutoDeleteTimeDeltaMonths();
        String settingsTitle =
                context.getString(R.string.archived_tab_iph_card_subtitle_settings_title);
        // The auto-delete section is blank when the feature param is disabled.
        String autoDeleteTitle =
                tabArchiveSettings.isAutoDeleteEnabled()
                        ? context.getString(
                                R.string.archived_tab_iph_card_subtitle_autodelete_section,
                                autoDeleteTimeDeltaMonths)
                        : "";
        int iphCardSubtitleRes =
                ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()
                        ? R.string.archived_tab_iph_card_subtitle_with_tab_groups
                        : R.string.archived_tab_iph_card_subtitle;
        String description =
                context.getString(
                        iphCardSubtitleRes, archiveTimeDeltaDays, autoDeleteTitle, settingsTitle);

        SpannableString ss = new SpannableString(description);
        ForegroundColorSpan fcs =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorAccent1(context));
        ss.setSpan(
                fcs,
                description.indexOf(settingsTitle),
                description.indexOf(settingsTitle) + settingsTitle.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return ss;
    }

    private List<String> getArchivedTabGroupSyncIds() {
        if (!ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()
                || mTabGroupSyncService == null) {
            return Collections.emptyList();
        }

        List<String> tabGroupSyncIds = new ArrayList<>();
        for (String syncGroupId : mTabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);

            if (savedTabGroup != null) {
                if (savedTabGroup.archivalTimeMs != null) {
                    tabGroupSyncIds.add(syncGroupId);
                }
            }
        }

        return tabGroupSyncIds;
    }

    private int getSyncedTabGroupTabsCount(List<String> archivedTabGroupSyncIds) {
        if (mTabGroupSyncService == null) return 0;

        int tabGroupTabCount = 0;
        for (String syncGroupId : archivedTabGroupSyncIds) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);

            if (savedTabGroup != null) {
                assert !savedTabGroup.savedTabs.isEmpty();
                tabGroupTabCount += savedTabGroup.savedTabs.size();
            }
        }

        return tabGroupTabCount;
    }

    private void closeArchivedTabGroups(List<String> archivedTabGroupSyncIds) {
        if (mTabGroupSyncService != null) {
            for (String syncGroupId : archivedTabGroupSyncIds) {
                mTabGroupSyncService.updateArchivalStatus(syncGroupId, false);
                if (mTabListEditorCoordinator != null) {
                    mTabListEditorCoordinator.removeListItem(
                            UiType.TAB_GROUP,
                            TabListEditorItemSelectionId.createTabGroupSyncId(syncGroupId));
                }
            }

            moveToState(TabActionState.CLOSABLE);
        }
    }

    // SnackbarManageable implementation.

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    // Testing-specific methods.

    void setTabListEditorCoordinatorForTesting(TabListEditorCoordinator tabListEditorCoordinator) {
        mTabListEditorCoordinator = tabListEditorCoordinator;
    }

    ArchiveDelegate getArchiveDelegateForTesting() {
        return mArchiveDelegate;
    }

    TabListEditorCoordinator.LifecycleObserver getTabListEditorLifecycleObserver() {
        return mTabListEditorLifecycleObserver;
    }

    View getViewForTesting() {
        return mDialogView;
    }

    GridCardOnClickListenerProvider getGridCardOnClickListenerProviderForTesting() {
        return mGridCardOnClickListenerProvider;
    }

    /** Returns the Edge to edge pad adjuster. */
    @Nullable
    EdgeToEdgePadAdjuster getEdgeToEdgePadAdjusterForTesting() {
        return mEdgeToEdgePadAdjuster;
    }

    @VisibleForTesting
    FrameLayout getCloseAllTabsButtonContainer() {
        return mDialogView.findViewById(R.id.close_all_tabs_button_container);
    }

    DestroyChecker getDestroyCheckerForTesting() {
        return mDestroyChecker;
    }
}
