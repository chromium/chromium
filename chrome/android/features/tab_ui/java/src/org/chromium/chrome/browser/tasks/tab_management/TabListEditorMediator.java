// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.LifecycleObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider.AppHeaderObserver;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * This class is the mediator that contains all business logic for TabListEditor component. It is
 * also responsible for resetting the selectable tab grid based on visibility property.
 */
class TabListEditorMediator
        implements TabListEditorCoordinator.TabListEditorController,
                TabListEditorAction.ActionDelegate,
                AppHeaderObserver {
    private final Context mContext;
    private final @NonNull ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final @NonNull ValueChangedCallback<TabModelFilter> mOnTabModelFilterChanged =
            new ValueChangedCallback<>(this::onTabModelFilterChanged);
    private final PropertyModel mModel;
    private final SelectionDelegate<Integer> mSelectionDelegate;
    private final boolean mActionOnRelatedTabs;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final List<Tab> mVisibleTabs = new ArrayList<>();
    private final TabListEditorLayout mTabListEditorLayout;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;

    private @Nullable TabListCoordinator mTabListCoordinator;
    private @Nullable TabListEditorCoordinator.ResetHandler mResetHandler;
    private PropertyListModel<PropertyModel, PropertyKey> mActionListModel;
    private ListModelChangeProcessor mActionChangeProcessor;
    private TabListEditorMenu mTabListEditorMenu;
    private SnackbarManager mSnackbarManager;
    private BottomSheetController mBottomSheetController;
    private TabListEditorToolbar mTabListEditorToolbar;
    private TabListEditorCoordinator.NavigationProvider mNavigationProvider;
    private @TabActionState int mTabActionState;
    private LifecycleObserver mLifecycleObserver;
    private int mSnackbarOverrideToken;

    private final View.OnClickListener mNavigationClickListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mNavigationProvider.goBack();
                }
            };

    TabListEditorMediator(
            Context context,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            PropertyModel model,
            SelectionDelegate<Integer> selectionDelegate,
            boolean actionOnRelatedTabs,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            TabListEditorLayout tabListEditorLayout,
            @TabActionState int initialTabActionState,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        mContext = context;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mModel = model;
        mSelectionDelegate = selectionDelegate;
        mActionOnRelatedTabs = actionOnRelatedTabs;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mTabListEditorLayout = tabListEditorLayout;
        mTabActionState = initialTabActionState;
        mDesktopWindowStateProvider = desktopWindowStateProvider;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) return;
                        // When tab is added due to
                        // 1) multi-window close
                        // 2) moving between multiple windows
                        // 3) NTP at startup
                        // force hiding the selection editor.
                        if (type == TabLaunchType.FROM_RESTORE
                                || type == TabLaunchType.FROM_REPARENTING
                                || type == TabLaunchType.FROM_STARTUP) {
                            mNavigationProvider.goBack();
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (mTabActionState != TabProperties.TabActionState.CLOSABLE) {
                            mNavigationProvider.goBack();
                        }
                    }

                    // TODO(crbug.com/40945153): Revisit after adding the inactive tab model for
                    // using a custom click handler when selecting tabs.
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (mTabActionState == TabProperties.TabActionState.CLOSABLE
                                && type == TabSelectionType.FROM_USER) {
                            mNavigationProvider.goBack();
                        }
                    }
                };

        mOnTabModelFilterChanged.onResult(
                mCurrentTabModelFilterSupplier.addObserver(mOnTabModelFilterChanged));

        mBackPressChangedSupplier.set(isEditorVisible());
        mModel.addObserver(
                (source, key) -> {
                    if (key == TabListEditorProperties.IS_VISIBLE) {
                        mBackPressChangedSupplier.set(isEditorVisible());
                    }
                });
        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.addObserver(this);
            if (mDesktopWindowStateProvider.getAppHeaderState() != null) {
                onAppHeaderStateChanged(mDesktopWindowStateProvider.getAppHeaderState());
            }
        }
    }

    private boolean isEditorVisible() {
        if (mTabListCoordinator == null) return false;
        return mModel.get(TabListEditorProperties.IS_VISIBLE);
    }

    private void updateColors(boolean isIncognito) {
        @ColorInt int primaryColor = ChromeColors.getPrimaryBackgroundColor(mContext, isIncognito);
        @ColorInt
        int toolbarBackgroundColor =
                TabUiThemeProvider.getTabSelectionToolbarBackground(mContext, isIncognito);
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

    public void initializeWithTabListCoordinator(
            TabListCoordinator tabListCoordinator,
            TabListEditorCoordinator.ResetHandler resetHandler) {
        mTabListCoordinator = tabListCoordinator;
        mTabListEditorToolbar = mTabListEditorLayout.getToolbar();
        mResetHandler = resetHandler;

        mModel.set(TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        if (mActionOnRelatedTabs) {
            mModel.set(
                    TabListEditorProperties.RELATED_TAB_COUNT_PROVIDER,
                    (tabIdList) -> {
                        return TabListEditorAction.getTabCountIncludingRelatedTabs(
                                (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                                tabIdList);
                    });
        }
        updateColors(mCurrentTabModelFilterSupplier.get().isIncognito());
    }

    /** {@link TabListEditorCoordinator.TabListEditorController} implementation. */
    @Override
    public void show(List<Tab> tabs, @Nullable RecyclerViewPosition recyclerViewPosition) {
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
        mVisibleTabs.clear();
        mVisibleTabs.addAll(tabs);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        mResetHandler.resetWithListOfTabs(tabs, recyclerViewPosition, /* quickMode= */ false);

        mModel.set(TabListEditorProperties.IS_VISIBLE, true);
        mModel.set(
                TabListEditorProperties.TOOLBAR_TITLE,
                mContext.getString(R.string.tab_selection_editor_toolbar_select_tabs));
        updateColors(mCurrentTabModelFilterSupplier.get().isIncognito());
    }

    @Override
    public void configureToolbarWithMenuItems(List<TabListEditorAction> actions) {
        // Deferred initialization.
        if (mActionListModel == null) {
            mActionListModel = new PropertyListModel<>();
            mTabListEditorMenu =
                    new TabListEditorMenu(
                            mContext, mTabListEditorToolbar.getActionViewLayout());
            mSelectionDelegate.addObserver(mTabListEditorMenu);
            mActionChangeProcessor =
                    new ListModelChangeProcessor(
                            mActionListModel,
                            mTabListEditorMenu,
                            new TabListEditorMenuAdapter());
            mActionListModel.addObserver(mActionChangeProcessor);
        }

        mActionListModel.clear();
        for (TabListEditorAction action : actions) {
            action.configure(
                    mCurrentTabModelFilterSupplier, mSelectionDelegate, this, mActionOnRelatedTabs);
            mActionListModel.add(action.getPropertyModel());
        }

        updateColors(mCurrentTabModelFilterSupplier.get().isIncognito());
    }

    @Override
    public boolean handleBackPressed() {
        if (!isEditorVisible()) return false;
        mNavigationProvider.goBack();
        return true;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int result = isEditorVisible() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
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
        mResetHandler.resetWithListOfTabs(
                null, /* recyclerViewPosition= */ null, /* quickMode= */ false);
        mModel.set(TabListEditorProperties.IS_VISIBLE, false);
        mResetHandler.postHiding();
        if (mLifecycleObserver != null) mLifecycleObserver.didHide();
    }

    @Override
    public boolean isVisible() {
        return isEditorVisible();
    }

    @Override
    public void setToolbarTitle(String title) {
        mModel.set(TabListEditorProperties.TOOLBAR_TITLE, title);
    }

    @Override
    public void setNavigationProvider(
            @NonNull TabListEditorCoordinator.NavigationProvider navigationProvider) {
        assert navigationProvider != null;
        mNavigationProvider = navigationProvider;
    }

    @Override
    public void setTabActionState(@TabActionState int tabActionState) {
        mTabActionState = tabActionState;
        mTabListCoordinator.setTabActionState(tabActionState);
    }

    @Override
    public void setLifecycleObserver(LifecycleObserver lifecycleObserver) {
        mLifecycleObserver = lifecycleObserver;
    }

    @Override
    public void selectAll() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        for (Tab tab : mVisibleTabs) {
            selectedTabIds.add(tab.getId());
        }
        mSelectionDelegate.setSelectedItems(selectedTabIds);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs, /* recyclerViewPosition= */ null, /* quickMode= */ true);
    }

    @Override
    public void deselectAll() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        selectedTabIds.clear();
        mSelectionDelegate.setSelectedItems(selectedTabIds);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs, /* recyclerViewPosition= */ null, /* quickMode= */ true);
    }

    @Override
    public boolean areAllTabsSelected() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        return selectedTabIds.size() == mVisibleTabs.size();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /** AppHeaderObserver implementation */
    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        mModel.set(TabListEditorProperties.TOP_MARGIN, newState.getAppHeaderHeight());
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        removeTabModelFilterObserver(mCurrentTabModelFilterSupplier.get());
        mCurrentTabModelFilterSupplier.removeObserver(mOnTabModelFilterChanged);
    }

    private void onTabModelFilterChanged(
            @Nullable TabModelFilter newFilter, @Nullable TabModelFilter oldFilter) {
        removeTabModelFilterObserver(oldFilter);

        if (newFilter != null) {
            // Incognito in both light/dark theme is the same as non-incognito mode in dark theme.
            // Non-incognito mode and incognito in both light/dark themes in dark theme all look
            // dark.
            updateColors(newFilter.isIncognito());
            newFilter.addObserver(mTabModelObserver);
        }
    }

    private void removeTabModelFilterObserver(@Nullable TabModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
        }
    }
}
