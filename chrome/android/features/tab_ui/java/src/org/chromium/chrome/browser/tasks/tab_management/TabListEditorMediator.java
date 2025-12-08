// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionProperties.DESTROYABLE;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.chrome_item_picker.TabItemPickerCoordinator.ItemPickerSelectionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.LifecycleObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * This class is the mediator that contains all business logic for TabListEditor component. It is
 * also responsible for resetting the selectable tab grid based on visibility property.
 */
@NullMarked
class TabListEditorMediator
        implements TabListEditorCoordinator.TabListEditorController,
                TabListEditorAction.ActionDelegate,
                AppHeaderObserver {
    private final Context mContext;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final PropertyModel mModel;
    private final SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    private final boolean mActionOnRelatedTabs;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final List<Tab> mVisibleTabs = new ArrayList<>();
    private final List<String> mVisibleTabGroups = new ArrayList<>();
    private final TabListEditorLayout mTabListEditorLayout;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final @CreationMode int mCreationMode;
    private final SelectionObserver<TabListEditorItemSelectionId> mSelectionObserver;

    private TabListCoordinator mTabListCoordinator;
    private TabListEditorCoordinator.ResetHandler mResetHandler;
    private @Nullable PropertyListModel<PropertyModel, PropertyKey> mActionListModel;
    private final SnackbarManager mSnackbarManager;
    private final @Nullable BottomSheetController mBottomSheetController;
    private TabListEditorToolbar mTabListEditorToolbar;
    private TabListEditorCoordinator.@Nullable NavigationProvider mNavigationProvider;
    private @TabActionState int mTabActionState;
    private @Nullable LifecycleObserver mLifecycleObserver;
    private int mSnackbarOverrideToken;
    private @Nullable ItemPickerSelectionHandler mSelectionHandler;
    private Set<TabListEditorItemSelectionId> mInitialSelectedTabIds = new HashSet<>();

    private final View.OnClickListener mNavigationClickListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    assumeNonNull(mNavigationProvider);
                    mNavigationProvider.goBack();
                }
            };

    private final View.OnClickListener mDoneButtonClickHandler =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    assumeNonNull(mSelectionHandler);

                    List<TabListEditorItemSelectionId> selectedItems =
                            new ArrayList<>(mSelectionDelegate.getSelectedItems());
                    mSelectionHandler.finishSelection(selectedItems);
                }
            };

    TabListEditorMediator(
            Context context,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            PropertyModel model,
            SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate,
            boolean actionOnRelatedTabs,
            SnackbarManager snackbarManager,
            @Nullable BottomSheetController bottomSheetController,
            TabListEditorLayout tabListEditorLayout,
            @TabActionState int initialTabActionState,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @CreationMode int creationMode) {
        mContext = context;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mModel = model;
        mSelectionDelegate = selectionDelegate;
        mActionOnRelatedTabs = actionOnRelatedTabs;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mTabListEditorLayout = tabListEditorLayout;
        mTabActionState = initialTabActionState;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mCreationMode = creationMode;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) return;
                        // When tab is added due to
                        // 1) multi-window close
                        // 2) moving between multiple windows
                        // 3) NTP at startup
                        // force hiding the selection editor.
                        if (type == TabLaunchType.FROM_RESTORE
                                || type == TabLaunchType.FROM_REPARENTING
                                || type == TabLaunchType.FROM_REPARENTING_BACKGROUND
                                || type == TabLaunchType.FROM_STARTUP) {
                            assumeNonNull(mNavigationProvider);
                            mNavigationProvider.goBack();
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (mTabActionState != TabProperties.TabActionState.CLOSABLE) {
                            assumeNonNull(mNavigationProvider);
                            mNavigationProvider.goBack();
                        }
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (mTabActionState == TabProperties.TabActionState.CLOSABLE
                                && type == TabSelectionType.FROM_USER) {
                            assumeNonNull(mNavigationProvider);
                            mNavigationProvider.goBack();
                        }
                    }
                };

        mSelectionObserver =
                new SelectionDelegate.SelectionObserver<>() {
                    @Override
                    public void onSelectionStateChange(
                            List<TabListEditorItemSelectionId> selectedItems) {
                        updateToolbar();
                    }
                };
        mSelectionDelegate.addObserver(mSelectionObserver);

        mCurrentTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(
                mOnTabGroupModelFilterChanged);

        mBackPressChangedSupplier.set(isEditorVisible());
        mModel.addObserver(
                (source, key) -> {
                    if (key == TabListEditorProperties.IS_VISIBLE) {
                        mBackPressChangedSupplier.set(isEditorVisible());
                    }
                });
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
            if (mDesktopWindowStateManager.getAppHeaderState() != null) {
                onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());
            }
        }
    }

    private boolean isEditorVisible() {
        if (mTabListCoordinator == null) return false;
        return mModel.get(TabListEditorProperties.IS_VISIBLE);
    }

    private void updateToolbar() {
        if (mCreationMode != CreationMode.ITEM_PICKER) {
            mModel.set(TabListEditorProperties.DONE_BUTTON_VISIBILITY, false);
            return;
        }
        mModel.set(TabListEditorProperties.DONE_BUTTON_VISIBILITY, true);

        Set<TabListEditorItemSelectionId> currentSelection = mSelectionDelegate.getSelectedItems();
        boolean hasSelectionChanged = !Objects.equals(mInitialSelectedTabIds, currentSelection);
        mModel.set(TabListEditorProperties.IS_DONE_BUTTON_ENABLED, hasSelectionChanged);
    }

    private void updateColors(boolean isIncognito) {
        @ColorInt
        int primaryColor =
                TabUiThemeProvider.getTabGridDialogBackgroundColor(
                        mContext, isIncognito, mCreationMode);
        @ColorInt
        int toolbarBackgroundColor =
                TabUiThemeProvider.getTabSelectionToolbarBackground(
                        mContext, isIncognito, mCreationMode);
        ColorStateList toolbarTintColorList =
                TabUiThemeProvider.getTabSelectionToolbarIconTintList(mContext, isIncognito);

        mModel.set(TabListEditorProperties.PRIMARY_COLOR, primaryColor);
        mModel.set(TabListEditorProperties.TOOLBAR_BACKGROUND_COLOR, toolbarBackgroundColor);
        mModel.set(TabListEditorProperties.TOOLBAR_TEXT_TINT, toolbarTintColorList);
        mModel.set(TabListEditorProperties.TOOLBAR_BUTTON_TINT, toolbarTintColorList);

        if (mActionListModel == null) return;

        for (PropertyModel model : mActionListModel) {
            model.set(TabListEditorActionProperties.TEXT_TINT, toolbarTintColorList);
            model.set(TabListEditorActionProperties.ICON_TINT, toolbarTintColorList);
        }
    }

    @Initializer
    public void initializeWithTabListCoordinator(
            TabListCoordinator tabListCoordinator,
            TabListEditorCoordinator.ResetHandler resetHandler) {
        mTabListCoordinator = tabListCoordinator;
        mTabListEditorToolbar = mTabListEditorLayout.getToolbar();
        mResetHandler = resetHandler;

        mModel.set(TabListEditorProperties.CREATION_MODE, mCreationMode);

        mModel.set(TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        mModel.set(TabListEditorProperties.DONE_BUTTON_CLICK_HANDLER, mDoneButtonClickHandler);
        updateColors(
                assumeNonNull(mCurrentTabGroupModelFilterSupplier.get())
                        .getTabModel()
                        .isIncognito());
    }

    /** {@link TabListEditorCoordinator.TabListEditorController} implementation. */
    @Override
    public void show(
            List<Tab> tabs,
            List<String> tabGroupSyncIds,
            @Nullable RecyclerViewPosition recyclerViewPosition) {
        assert mNavigationProvider != null : "NavigationProvider must be set before calling #show";
        // Reparent the snackbarManager to use the selection editor layout to avoid layering issues.
        mSnackbarOverrideToken =
                mSnackbarManager.pushParentViewToOverrideStack(mTabListEditorLayout);
        // Records to a histogram the time since an instance of TabListEditor was last opened
        // within an activity lifespan.
        TabUiMetricsHelper.recordEditorTimeSinceLastShownHistogram();
        // We don't call TabListCoordinator#prepareTabSwitcherView, since not all the logic (e.g.
        // requiring one tab to be selected) is applicable here.
        mTabListCoordinator.prepareTabGridView();
        mTabListCoordinator.attachEmptyView();
        mVisibleTabs.clear();
        mVisibleTabs.addAll(tabs);
        mVisibleTabGroups.clear();
        mVisibleTabGroups.addAll(tabGroupSyncIds);

        mResetHandler.resetWithListOfTabs(
                tabs, tabGroupSyncIds, recyclerViewPosition, /* quickMode= */ false);
        mTabListEditorLayout.hideLoadingUi();

        mModel.set(TabListEditorProperties.IS_VISIBLE, true);

        updateToolbar();

        updateColors(
                assumeNonNull(mCurrentTabGroupModelFilterSupplier.get())
                        .getTabModel()
                        .isIncognito());
    }

    @Override
    public void configureToolbarWithMenuItems(List<TabListEditorAction> actions) {
        // Deferred initialization.
        if (mActionListModel == null) {
            mActionListModel = new PropertyListModel<>();
            TabListEditorMenu menu =
                    new TabListEditorMenu(mContext, mTabListEditorToolbar.getActionViewLayout());
            mSelectionDelegate.addObserver(menu);
            ListModelChangeProcessor actionChangeProcessor =
                    new ListModelChangeProcessor(
                            mActionListModel, menu, new TabListEditorMenuAdapter());
            mActionListModel.addObserver(actionChangeProcessor);
        }

        runListDestroyables();
        mActionListModel.clear();
        for (TabListEditorAction action : actions) {
            action.configure(
                    mCurrentTabGroupModelFilterSupplier,
                    mSelectionDelegate,
                    this,
                    mActionOnRelatedTabs);
            mActionListModel.add(action.getPropertyModel());
        }

        updateColors(
                assumeNonNull(mCurrentTabGroupModelFilterSupplier.get())
                        .getTabModel()
                        .isIncognito());
    }

    @Override
    public boolean handleBackPressed() {
        if (!isEditorVisible()) return false;
        assumeNonNull(mNavigationProvider);
        mNavigationProvider.goBack();
        return true;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int result = isEditorVisible() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        assumeNonNull(mNavigationProvider);
        mNavigationProvider.goBack();
        return result;
    }

    @Override
    public void syncRecyclerViewPosition() {
        mResetHandler.syncRecyclerViewPosition();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void hide() {
        hideInternal(/* hiddenByAction= */ false);
    }

    @Override
    public void hideByAction() {
        hideInternal(/* hiddenByAction= */ true);
    }

    private void hideInternal(boolean hiddenByAction) {
        if (!isEditorVisible()) return;
        if (mLifecycleObserver != null) mLifecycleObserver.willHide();
        mSnackbarManager.popParentViewFromOverrideStack(mSnackbarOverrideToken);
        mSnackbarOverrideToken = TokenHolder.INVALID_TOKEN;
        TabUiMetricsHelper.recordSelectionEditorExitMetrics(
                TabListEditorExitMetricGroups.CLOSED, mContext);

        // When hiding by action it is expected that syncRecyclerViewPosition() is called before the
        // action occurs. This is because an action may remove tabs so sync position must happen
        // first so the recyclerViewStat is valid due to the same number of items.
        if (!hiddenByAction) {
            syncRecyclerViewPosition();
        }
        mTabListCoordinator.cleanupTabGridView();
        mVisibleTabs.clear();
        mVisibleTabGroups.clear();

        if (mCreationMode != CreationMode.ITEM_PICKER) {
            mResetHandler.resetWithListOfTabs(
                    /* tabs= */ null,
                    /* tabGroupSyncIds= */ null,
                    /* recyclerViewPosition= */ null,
                    /* quickMode= */ false);
            mModel.set(TabListEditorProperties.IS_VISIBLE, false);
            mResetHandler.postHiding();
        }
        if (mLifecycleObserver != null) mLifecycleObserver.didHide();
    }

    @Override
    public boolean isVisible() {
        return isEditorVisible();
    }

    @Override
    public boolean needsCleanUp() {
        return false;
    }

    @Override
    public void setToolbarTitle(String title) {
        mModel.set(TabListEditorProperties.TOOLBAR_TITLE, title);
    }

    @Override
    public void setNavigationProvider(
            TabListEditorCoordinator.NavigationProvider navigationProvider) {
        assert navigationProvider != null;
        mNavigationProvider = navigationProvider;
    }

    @Override
    public void setTabActionState(@TabActionState int tabActionState) {
        mTabActionState = tabActionState;
        mTabListCoordinator.setTabActionState(tabActionState);
    }

    @Override
    public void setLifecycleObserver(@Nullable LifecycleObserver lifecycleObserver) {
        mLifecycleObserver = lifecycleObserver;
    }

    @Override
    public void selectAll() {
        Set<TabListEditorItemSelectionId> selectedItemIds = mSelectionDelegate.getSelectedItems();
        for (Tab tab : mVisibleTabs) {
            selectedItemIds.add(TabListEditorItemSelectionId.createTabId(tab.getId()));
        }
        selectTabs(selectedItemIds);
    }

    @Override
    public void deselectAll() {
        Set<TabListEditorItemSelectionId> selectedItemIds = mSelectionDelegate.getSelectedItems();
        selectedItemIds.clear();
        mSelectionDelegate.setSelectedItems(selectedItemIds);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs,
                mVisibleTabGroups.isEmpty() ? null : mVisibleTabGroups,
                /* recyclerViewPosition= */ null,
                /* quickMode= */ true);
    }

    @Override
    public boolean areAllTabsSelected() {
        Set<TabListEditorItemSelectionId> selectedItemIds = mSelectionDelegate.getSelectedItems();
        return selectedItemIds.size() == mVisibleTabs.size();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    public @Nullable BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /** AppHeaderObserver implementation */
    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        mModel.set(TabListEditorProperties.TOP_MARGIN, newState.getAppHeaderHeight());
    }

    @Override
    public void selectTabs(Set<TabListEditorItemSelectionId> itemIds) {
        // Protects selection delegate from immutable sets.
        Set<TabListEditorItemSelectionId> itemIdsModifiable = new HashSet<>(itemIds);
        mSelectionDelegate.setSelectedItems(itemIdsModifiable);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs,
                mVisibleTabGroups.isEmpty() ? null : mVisibleTabGroups,
                /* recyclerViewPosition= */ null,
                /* quickMode= */ true);
    }

    @Override
    public void preselectTabs(Set<TabListEditorItemSelectionId> itemIds) {
        mInitialSelectedTabIds = itemIds;
        selectTabs(itemIds);
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        runListDestroyables();

        removeTabGroupModelFilterObserver(assumeNonNull(mCurrentTabGroupModelFilterSupplier.get()));
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);

        mSelectionDelegate.removeObserver(mSelectionObserver);

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
    }

    @Override
    public void setSelectionHandler(ItemPickerSelectionHandler selectionHandler) {
        mSelectionHandler = selectionHandler;
    }

    private void runListDestroyables() {
        if (mActionListModel == null) return;
        for (PropertyModel model : mActionListModel) {
            @Nullable Destroyable destroyable = model.get(DESTROYABLE);
            if (destroyable != null) {
                destroyable.destroy();
            }
        }
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeTabGroupModelFilterObserver(oldFilter);

        if (newFilter != null) {
            // Incognito in both light/dark theme is the same as non-incognito mode in dark theme.
            // Non-incognito mode and incognito in both light/dark themes in dark theme all look
            // dark.
            updateColors(newFilter.getTabModel().isIncognito());
            newFilter.addObserver(mTabModelObserver);
        }
    }

    private void removeTabGroupModelFilterObserver(@Nullable TabGroupModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
        }
    }
}
