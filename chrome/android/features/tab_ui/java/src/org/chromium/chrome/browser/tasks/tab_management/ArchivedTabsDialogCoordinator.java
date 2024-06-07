// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.List;

public class ArchivedTabsDialogCoordinator {

    private final NavigationProvider mNavigationProvider =
            new NavigationProvider() {
                @Override
                public void goBack() {
                    if (mTabActionState == TabActionState.CLOSABLE) {
                        hide();
                    } else {
                        // TODO(crbug.com/342255180): Enable this when TabListEditor supports
                        // setting action state.
                    }
                }
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void willCloseTab(Tab tab, boolean didCloseAlone) {
                    if (mArchivedTabModel.getCount() <= 0) {
                        hide();
                        return;
                    }

                    updateTitle();
                }

                @Override
                public void willAddTab(Tab tab, @TabLaunchType int type) {
                    updateTitle();
                }
            };

    private final @NonNull Context mContext;
    private final @NonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @NonNull TabModel mArchivedTabModel;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull TabContentManager mTabContentManager;
    private final @TabListMode int mMode;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;

    private ViewGroup mView;
    private @TabActionState int mTabActionState = TabActionState.CLOSABLE;
    private TabListEditorCoordinator mTabListEditorCoordinator;

    /**
     * @param context The android context.
     * @param archivedTabModelOrchestrator The TabModelOrchestrator for archived tabs.
     * @param browserControlsStateProvider Used as a dependency to TabListEditorCoordiantor.
     * @param tabContentManager Used as a dependency to TabListEditorCoordiantor.
     * @param mode Used as a dependency to TabListEditorCoordiantor.
     * @param rootView Used as a dependency to TabListEditorCoordiantor.
     */
    public ArchivedTabsDialogCoordinator(
            @NonNull Context context,
            @NonNull ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabContentManager tabContentManager,
            @TabListMode int mode,
            @NonNull ViewGroup rootView,
            @NonNull SnackbarManager snackbarManager) {
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mMode = mode;
        mRootView = rootView;
        mSnackbarManager = snackbarManager;

        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mArchivedTabModel =
                mArchivedTabModelOrchestrator
                        .getTabModelSelector()
                        .getModel(/* incognito= */ false);
        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.archived_tabs_dialog, mRootView, false);
    }

    public void show() {
        if (mTabListEditorCoordinator == null) {
            createTabListEditorCoordinator();
        }

        mArchivedTabModel.addObserver(mTabModelObserver);

        List<Tab> archivedTabs = TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel);
        mTabListEditorCoordinator.getController().show(archivedTabs, 0, null);
        mRootView.addView(mView);
        // TODO(crbug.com/345789067): Also configure the menu items that are shown.

        mTabListEditorCoordinator.getController().setNavigationProvider(mNavigationProvider);
        updateTitle();
    }

    public void hide() {
        mTabListEditorCoordinator.getController().hide();
        mRootView.removeView(mView);
        mArchivedTabModel.removeObserver(mTabModelObserver);
    }

    @VisibleForTesting
    void updateTitle() {
        int numInactiveTabs = mArchivedTabModel.getCount();
        String title =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.archived_tabs_dialog_title,
                                numInactiveTabs,
                                numInactiveTabs);
        mTabListEditorCoordinator.getController().setToolbarTitle(title);
    }

    private void createTabListEditorCoordinator() {
        mTabListEditorCoordinator =
                new TabListEditorCoordinator(
                        mContext,
                        /* parentView= */ mView,
                        mBrowserControlsStateProvider,
                        mArchivedTabModelOrchestrator
                                .getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilterSupplier(),
                        mTabContentManager,
                        /* clientTabListRecyclerViewPositionSetter= */ null,
                        mMode,
                        mRootView,
                        /* displayGroups= */ false,
                        mSnackbarManager,
                        TabProperties.TabActionState.CLOSABLE);
    }

    // Testing-specific methods

    void setTabListEditorCoordinatorForTesting(TabListEditorCoordinator tabListEditorCoordinator) {
        mTabListEditorCoordinator = tabListEditorCoordinator;
    }
}
