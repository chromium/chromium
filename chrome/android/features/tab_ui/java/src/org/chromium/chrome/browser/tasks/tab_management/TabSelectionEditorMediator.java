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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider;
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
 * This class is the mediator that contains all business logic for TabSelectionEditor component. It
 * is also responsible for resetting the selectable tab grid based on visibility property.
 */
class TabSelectionEditorMediator
        implements TabSelectionEditorCoordinator.TabSelectionEditorController,
                   TabSelectionEditorAction.ActionDelegate {
    // TODO(977271): Unify similar interfaces in other components that used the TabListCoordinator.
    /**
     * Interface for resetting the selectable tab grid.
     */
    interface ResetHandler {
        /**
         * Handles the reset event.
         * @param tabs List of {@link Tab}s to reset.
         * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
         * @param quickMode whether to use quick mode.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount, boolean quickMode);
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final TabListCoordinator mTabListCoordinator;
    private final ResetHandler mResetHandler;
    private final PropertyModel mModel;
    private final SelectionDelegate<Integer> mSelectionDelegate;
    private final TabSelectionEditorToolbar mTabSelectionEditorToolbar;
    private final boolean mActionOnRelatedTabs;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private TabSelectionEditorActionProvider mActionProvider;
    private TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider mNavigationProvider;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final List<Tab> mVisibleTabs = new ArrayList<>();

    private PropertyListModel<PropertyModel, PropertyKey> mActionListModel;
    private ListModelChangeProcessor mActionChangeProcessor;
    private TabSelectionEditorMenu mTabSelectionEditorMenu;

    private final View.OnClickListener mNavigationClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            mNavigationProvider.goBack();
        }
    };

    private final View.OnClickListener mActionButtonOnClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            List<Tab> selectedTabs = new ArrayList<>();

            for (int tabId : mSelectionDelegate.getSelectedItems()) {
                selectedTabs.add(
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
            }

            if (mActionProvider == null) return;
            mActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);
        }
    };

    TabSelectionEditorMediator(Context context, TabModelSelector tabModelSelector,
            TabListCoordinator tabListCoordinator, ResetHandler resetHandler, PropertyModel model,
            SelectionDelegate<Integer> selectionDelegate,
            TabSelectionEditorToolbar tabSelectionEditorToolbar, boolean actionOnRelatedTabs) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mTabListCoordinator = tabListCoordinator;
        mResetHandler = resetHandler;
        mModel = model;
        mSelectionDelegate = selectionDelegate;
        mTabSelectionEditorToolbar = tabSelectionEditorToolbar;
        mActionOnRelatedTabs = actionOnRelatedTabs;

        mModel.set(
                TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER,
                mActionButtonOnClickListener);
        if (mActionOnRelatedTabs) {
            mModel.set(TabSelectionEditorProperties.RELATED_TAB_COUNT_PROVIDER, (tabIdList) -> {
                return TabSelectionEditorAction.getTabCountIncludingRelatedTabs(
                        mTabModelSelector, tabIdList);
            });
        }

        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState) {
                if (!mTabModelSelector.isTabStateInitialized()) return;
                // When tab is added due to multi-window close or moving between multiple windows,
                // force hiding the selection editor.
                if (type == TabLaunchType.FROM_RESTORE || type == TabLaunchType.FROM_REPARENTING) {
                    hide();
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                if (isEditorVisible()) hide();
            }
        };

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                // Incognito in both light/dark theme is the same as non-incognito mode in dark
                // theme. Non-incognito mode and incognito in both light/dark themes in dark theme
                // all look dark.
                updateColors(newModel.isIncognito());
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        updateColors(mTabModelSelector.isIncognitoSelected());

        // Default action for action button is to group selected tabs.
        mActionProvider = new TabSelectionEditorActionProvider(
                this, TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP);

        mNavigationProvider =
                new TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider(this);
        mBackPressChangedSupplier.set(isEditorVisible());
        mModel.addObserver((source, key) -> {
            if (key == TabSelectionEditorProperties.IS_VISIBLE) {
                mBackPressChangedSupplier.set(isEditorVisible());
            }
        });
    }

    private boolean isEditorVisible() {
        return mModel.get(TabSelectionEditorProperties.IS_VISIBLE);
    }

    private void updateColors(boolean isIncognito) {
        @ColorInt
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(mContext, isIncognito);
        @ColorInt
        int toolbarBackgroundColor =
                TabUiThemeProvider.getTabSelectionToolbarBackground(mContext, isIncognito);
        ColorStateList toolbarTintColorList =
                TabUiThemeProvider.getTabSelectionToolbarIconTintList(mContext, isIncognito);

        mModel.set(TabSelectionEditorProperties.PRIMARY_COLOR, primaryColor);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_BACKGROUND_COLOR, toolbarBackgroundColor);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_GROUP_TEXT_TINT, toolbarTintColorList);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_GROUP_BUTTON_TINT, toolbarTintColorList);

        if (mActionListModel == null) return;

        for (PropertyModel model : mActionListModel) {
            // TODO(ckitagawa): update these tints with input from UX.
            model.set(TabSelectionEditorActionProperties.TEXT_TINT, toolbarTintColorList);
            if (model.get(TabSelectionEditorActionProperties.SKIP_ICON_TINT)) continue;

            model.set(TabSelectionEditorActionProperties.ICON_TINT, toolbarTintColorList);
        }
    }

    /**
     * {@link TabSelectionEditorCoordinator.TabSelectionEditorController} implementation.
     */
    @Override
    public void show(List<Tab> tabs) {
        show(tabs, 0);
    }

    @Override
    public void show(List<Tab> tabs, int preSelectedTabCount) {
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

        mResetHandler.resetWithListOfTabs(tabs, preSelectedTabCount, /*quickMode=*/false);

        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, true);
    }

    @Override
    public void configureToolbar(@Nullable String actionButtonText,
            @Nullable Integer actionButtonDescriptionResourceId,
            @Nullable TabSelectionEditorActionProvider actionProvider,
            int actionButtonEnablingThreshold,
            @Nullable TabSelectionEditorNavigationProvider navigationProvider) {
        if (actionButtonText != null) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT, actionButtonText);
        }
        if (actionProvider != null) {
            mActionProvider = actionProvider;
        }
        if (actionButtonEnablingThreshold != -1) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD,
                    actionButtonEnablingThreshold);
        }
        if (navigationProvider != null) {
            mNavigationProvider = navigationProvider;
        }
        if (actionButtonDescriptionResourceId != null) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_DESCRIPTION_RESOURCE_ID,
                    actionButtonDescriptionResourceId);
        }
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_VISIBILITY, View.VISIBLE);
        updateColors(mTabModelSelector.isIncognitoSelected());
    }

    @Override
    public void configureToolbarWithMenuItems(List<TabSelectionEditorAction> actions,
            @Nullable TabSelectionEditorNavigationProvider navigationProvider) {
        // Deferred initialization.
        // TODO(ckitagawa): Move this to TabSelectionEditorCoordinator once it is lazily
        // initialized.
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_SELECTION_EDITOR_V2);
        if (mActionListModel == null) {
            mActionListModel = new PropertyListModel<>();
            mTabSelectionEditorMenu =
                    new TabSelectionEditorMenu(mContext, mTabSelectionEditorToolbar.getMenu());
            mTabSelectionEditorToolbar.setOnMenuItemClickListener(
                    mTabSelectionEditorMenu::onMenuItemClick);
            mSelectionDelegate.addObserver(mTabSelectionEditorMenu);
            mActionChangeProcessor = new ListModelChangeProcessor(
                    mActionListModel, mTabSelectionEditorMenu, new TabSelectionEditorMenuAdapter());
            mActionListModel.addObserver(mActionChangeProcessor);
        }

        mActionListModel.clear();
        for (TabSelectionEditorAction action : actions) {
            action.configure(mTabModelSelector, mSelectionDelegate, this, mActionOnRelatedTabs);
            mActionListModel.add(action.getPropertyModel());
        }
        if (navigationProvider != null) {
            mNavigationProvider = navigationProvider;
        }
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_VISIBILITY, View.GONE);
        updateColors(mTabModelSelector.isIncognitoSelected());
    }

    @Override
    public boolean handleBackPressed() {
        if (!isEditorVisible()) return false;
        mNavigationProvider.goBack();
        return true;
    }

    @Override
    public void handleBackPress() {
        mNavigationProvider.goBack();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void hide() {
        mTabListCoordinator.cleanupTabGridView();
        mVisibleTabs.clear();
        mResetHandler.resetWithListOfTabs(null, 0, /*quickMode=*/false);
        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, false);
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
        mResetHandler.resetWithListOfTabs(mVisibleTabs, mVisibleTabs.size(), /*quickMode=*/true);
    }

    @Override
    public void deselectAll() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        selectedTabIds.clear();
        mSelectionDelegate.setSelectedItems(selectedTabIds);
        mResetHandler.resetWithListOfTabs(mVisibleTabs, 0, /*quickMode=*/true);
    }

    @Override
    public boolean areAllTabsSelected() {
        Set<Integer> selectedTabIds = mSelectionDelegate.getSelectedItems();
        return selectedTabIds.size() == mVisibleTabs.size();
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelObserver.destroy();
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
    }
}
