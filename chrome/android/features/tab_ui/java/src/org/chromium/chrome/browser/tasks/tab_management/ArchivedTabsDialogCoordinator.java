// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.ArrayList;
import java.util.List;

public class ArchivedTabsDialogCoordinator {

    /** Interface exposing functionality to the menu items for the archived tabs dialog */
    public interface ArchiveDelegate {
        /** Restore all tabs from the archived tab model. */
        void restoreAllArchivedTabs();

        /** Open the archive settings page. */
        void openArchiveSettings();

        /** Start tab selection process. */
        void startTabSelection();
    }

    private final ArchiveDelegate mArchiveDelegate =
            new ArchiveDelegate() {
                @Override
                public void restoreAllArchivedTabs() {
                    while (mArchivedTabModel.getCount() > 0) {
                        mArchivedTabModelOrchestrator
                                .getTabArchiver()
                                .unarchiveAndRestoreTab(
                                        mRegularTabCreator, mArchivedTabModel.getTabAt(0));
                    }
                }

                @Override
                public void openArchiveSettings() {
                    new SettingsLauncherImpl()
                            .launchSettingsActivity(mContext, TabArchiveSettingsFragment.class);
                }

                @Override
                public void startTabSelection() {
                    moveToState(TabActionState.SELECTABLE);
                }
            };

    private final NavigationProvider mNavigationProvider =
            new NavigationProvider() {
                @Override
                public void goBack() {
                    if (mTabActionState == TabActionState.CLOSABLE) {
                        hide();
                    } else {
                        moveToState(TabActionState.CLOSABLE);
                    }
                }
            };

    private final Callback<Integer> mTabCountObserver =
            (count) -> {
                if (count <= 0) {
                    hide();
                    return;
                }
                updateTitle();
            };

    private final @NonNull Context mContext;
    private final @NonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @NonNull TabModel mArchivedTabModel;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull TabContentManager mTabContentManager;
    private final @TabListMode int mMode;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull TabCreator mRegularTabCreator;

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
            @NonNull SnackbarManager snackbarManager,
            @NonNull TabCreator regularTabCreator) {
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mMode = mode;
        mRootView = rootView;
        mSnackbarManager = snackbarManager;
        mRegularTabCreator = regularTabCreator;

        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mArchivedTabModel =
                mArchivedTabModelOrchestrator
                        .getTabModelSelector()
                        .getModel(/* incognito= */ false);
        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.archived_tabs_dialog, mRootView, false);
        mView.findViewById(R.id.close_all_tabs_button)
                .setOnClickListener(this::closeAllInactiveTabs);
    }

    public void show() {
        if (mTabListEditorCoordinator == null) {
            createTabListEditorCoordinator();
        }

        mArchivedTabModel.getTabCountSupplier().addObserver(mTabCountObserver);

        mRootView.addView(mView);
        List<Tab> archivedTabs = TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel);
        mTabListEditorCoordinator.getController().show(archivedTabs, 0, null);
        // View is obscured by the TabListEditorCoordinator, so it needs to be brought to the front.
        mView.findViewById(R.id.close_all_tabs_button_container).bringToFront();

        mTabListEditorCoordinator.getController().setNavigationProvider(mNavigationProvider);

        List<TabListEditorAction> actions = new ArrayList<>();
        actions.add(
                TabListEditorRestoreAllArchivedTabsAction.createAction(mContext, mArchiveDelegate));
        actions.add(TabListEditorSelectTabsAction.createAction(mContext, mArchiveDelegate));
        actions.add(TabListEditorArchiveSettingsAction.createAction(mContext, mArchiveDelegate));
        mTabListEditorCoordinator.getController().configureToolbarWithMenuItems(actions);

        updateTitle();
    }

    public void hide() {
        mTabListEditorCoordinator.getController().hide();
        mRootView.removeView(mView);
        mArchivedTabModel.getTabCountSupplier().removeObserver(mTabCountObserver);
    }

    void moveToState(@TabActionState int tabActionState) {
        mTabActionState = tabActionState;
        mTabListEditorCoordinator.getController().setTabActionState(mTabActionState);
        if (mTabActionState == TabActionState.CLOSABLE) {
            updateTitle();
        }
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

    @VisibleForTesting
    void closeAllInactiveTabs(View view) {
        mArchivedTabModel.closeAllTabs(false);
        hide();
    }

    // Testing-specific methods

    void setTabListEditorCoordinatorForTesting(TabListEditorCoordinator tabListEditorCoordinator) {
        mTabListEditorCoordinator = tabListEditorCoordinator;
    }

    ArchiveDelegate getArchiveDelegateForTesting() {
        return mArchiveDelegate;
    }
}
