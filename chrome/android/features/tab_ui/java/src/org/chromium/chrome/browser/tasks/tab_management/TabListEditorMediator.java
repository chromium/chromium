// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorNavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class is the mediator that contains all business logic for TabListEditor component. It
 * is also responsible for resetting the selectable tab grid based on visibility property.
 */
class TabListEditorMediator
        implements TabListEditorCoordinator.TabListEditorController,
                TabListEditorAction.ActionDelegate {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final TabListCoordinator mTabListCoordinator;
    private final TabListEditorCoordinator.ResetHandler mResetHandler;
    private final PropertyModel mModel;
    private final SelectionDelegate<Integer> mSelectionDelegate;
    private final TabListEditorToolbar mTabListEditorToolbar;
    private final boolean mActionOnRelatedTabs;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private TabListEditorCoordinator.TabListEditorNavigationProvider mNavigationProvider;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final List<Tab> mVisibleTabs = new ArrayList<>();

    private PropertyListModel<PropertyModel, PropertyKey> mActionListModel;
    private ListModelChangeProcessor mActionChangeProcessor;
    private TabListEditorMenu mTabListEditorMenu;
    private SnackbarManager mSnackbarManager;
    private TabListEditorLayout mTabListEditorLayout;

    private final View.OnClickListener mNavigationClickListener =
            new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mNavigationProvider.goBack();
                }
            };

    TabListEditorMediator(
            Context context,
            TabModelSelector tabModelSelector,
            TabListCoordinator tabListCoordinator,
            TabListEditorCoordinator.ResetHandler resetHandler,
            PropertyModel model,
            SelectionDelegate<Integer> selectionDelegate,
            TabListEditorToolbar tabListEditorToolbar,
            boolean actionOnRelatedTabs,
            SnackbarManager snackbarManager,
            TabListEditorLayout tabListEditorLayout,
            @UiType int itemType) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mTabListCoordinator = tabListCoordinator;
        mResetHandler = resetHandler;
        mModel = model;
        mSelectionDelegate = selectionDelegate;
        mTabListEditorToolbar = tabListEditorToolbar;
        mActionOnRelatedTabs = actionOnRelatedTabs;
        mSnackbarManager = snackbarManager;
        mTabListEditorLayout = tabListEditorLayout;

        mModel.set(
                TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        if (mActionOnRelatedTabs) {
            mModel.set(
                    TabListEditorProperties.RELATED_TAB_COUNT_PROVIDER,
                    (tabIdList) -> {
                        return TabListEditorAction.getTabCountIncludingRelatedTabs(
                                mTabModelSelector, tabIdList);
                    });
        }

        mTabModelObserver =
                new TabModelSelectorTabModelObserver(mTabModelSelector) {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        if (!mTabModelSelector.isTabStateInitialized()) return;
                        // When tab is added due to multi-window close or moving between multiple
                        // windows, force hiding the selection editor.
                        if (type == TabLaunchType.FROM_RESTORE
                                || type == TabLaunchType.FROM_REPARENTING) {
                            hide();
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                        if (itemType != TabProperties.UiType.CLOSABLE) {
                            hide();
                        }
                    }

                    // TODO(crbug.com/1504605): Revisit after adding the inactive tab model for
                    // using a custom click handler when selecting tabs.
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (itemType == TabProperties.UiType.CLOSABLE
                                && type == TabSelectionType.FROM_USER) {
                            hide();
                        }
                    }
                };

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        // Incognito in both light/dark theme is the same as non-incognito mode in
                        // dark theme. Non-incognito mode and incognito in both light/dark themes
                        // in dark theme all look dark.
                        updateColors(newModel.isIncognito());
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        updateColors(mTabModelSelector.isIncognitoSelected());

        mNavigationProvider =
                new TabListEditorCoordinator.TabListEditorNavigationProvider(
                        context, this);
        mBackPressChangedSupplier.set(isEditorVisible());
        mModel.addObserver(
                (source, key) -> {
                    if (key == TabListEditorProperties.IS_VISIBLE) {
                        mBackPressChangedSupplier.set(isEditorVisible());
                    }
                });
    }

    private boolean isEditorVisible() {
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

    /** {@link TabListEditorCoordinator.TabListEditorController} implementation. */
    @Override
    public void show(
            List<Tab> tabs,
            int preSelectedTabCount,
            @Nullable RecyclerViewPosition recyclerViewPosition) {
        // Reparent the snackbarManager to use the selection editor layout to avoid layering issues.
        mSnackbarManager.setParentView(mTabListEditorLayout);
        // Records to a histogram the time since an instance of TabListEditor was last opened
        // within an activity lifespan.
        TabUiMetricsHelper.recordEditorTimeSinceLastShownHistogram();
        // We don't call TabListCoordinator#prepareTabSwitcherView, since not all the logic (e.g.
        // requiring one tab to be selected) is applicable here.
        mTabListCoordinator.prepareTabGridView();
        mVisibleTabs.clear();
        mVisibleTabs.addAll(tabs);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        if (preSelectedTabCount > 0) {
            assert preSelectedTabCount <= tabs.size();

            Set<Integer> preSelectedTabIds = new HashSet<>();

            for (int i = 0; i < preSelectedTabCount; i++) {
                preSelectedTabIds.add(tabs.get(i).getId());
            }

            mSelectionDelegate.setSelectedItems(preSelectedTabIds);
        }

        mResetHandler.resetWithListOfTabs(
                tabs, preSelectedTabCount, recyclerViewPosition, /* quickMode= */ false);

        mModel.set(TabListEditorProperties.IS_VISIBLE, true);
    }

    @Override
    public void configureToolbarWithMenuItems(
            List<TabListEditorAction> actions,
            @Nullable TabListEditorNavigationProvider navigationProvider) {
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
            action.configure(mTabModelSelector, mSelectionDelegate, this, mActionOnRelatedTabs);
            mActionListModel.add(action.getPropertyModel());
        }
        if (navigationProvider != null) {
            mNavigationProvider = navigationProvider;
        }
        updateColors(mTabModelSelector.isIncognitoSelected());
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
        mSnackbarManager.setParentView(null);
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
                null,
                /* preSelectedCount= */ 0,
                /* recyclerViewPosition= */ null,
                /* quickMode= */ false);
        mModel.set(TabListEditorProperties.IS_VISIBLE, false);
        mResetHandler.postHiding();
    }

    @Override
    public boolean isVisible() {
        return isEditorVisible();
    }

    @Override
    public void selectAll() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        for (Tab tab : mVisibleTabs) {
            selectedTabIds.add(tab.getId());
        }
        mSelectionDelegate.setSelectedItems(selectedTabIds);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs,
                mVisibleTabs.size(),
                /* recyclerViewPosition= */ null,
                /* quickMode= */ true);
    }

    @Override
    public void deselectAll() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        selectedTabIds.clear();
        mSelectionDelegate.setSelectedItems(selectedTabIds);
        mResetHandler.resetWithListOfTabs(
                mVisibleTabs,
                /* preSelectedCount= */ 0,
                /* recyclerViewPosition= */ null,
                /* quickMode= */ true);
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

    /** Destroy any members that needs clean up. */
    public void destroy() {
        mTabModelObserver.destroy();
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
    }
}
