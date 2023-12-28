// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorNavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorOpenMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages the {@link TabListEditorCoordinator} and related components for a {@link TabSwitcher}.
 */
public class TabListEditorManager {
    private final @NonNull Activity mActivity;
    private final @NonNull ViewGroup mCoordinatorView;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final @NonNull Supplier<TabModel> mRegularTabModelSupplier;
    private final @NonNull TabContentManager mTabContentManager;
    private final @NonNull TabListCoordinator mTabListCoordinator;
    private final @TabListMode int mMode;
    private final @NonNull ObservableSupplierImpl<TabListEditorController> mControllerSupplier =
            new ObservableSupplierImpl<>();

    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;
    private @Nullable List<TabListEditorAction> mTabListEditorActions;

    /**
     * @param activity The current activity.
     * @param coordinatorView The overlay view to attach the editor to.
     * @param rootView The root view to attach the snackbar to.
     * @param browserControlsStateProvider The browser controls state provider.
     * @param currentTabModelFilterSupplier The supplier of the current {@link TabModelFilter}.
     * @param regularTabModelSupplier The supplier of the regular {@link TabModel}.
     * @param tabContentManager The {@link TabContentManager} for thumbnails.
     * @param tabListCoordinator The parent {@link TabListCoordinator}.
     * @param mode The {@link TabListMode} of the tab list (grid, list, etc.).
     */
    public TabListEditorManager(
            @NonNull Activity activity,
            @NonNull ViewGroup coordinatorView,
            @NonNull ViewGroup rootView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            @NonNull Supplier<TabModel> regularTabModelSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabListCoordinator tabListCoordinator,
            @TabListMode int mode) {
        mActivity = activity;
        mCoordinatorView = coordinatorView;
        mRootView = rootView;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mRegularTabModelSupplier = regularTabModelSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mTabListCoordinator = tabListCoordinator;
        mMode = mode;

        // The snackbarManager used by mTabListEditorCoordinator. The rootView is the default
        // default parent view of the snackbar. When shown this will be re-parented inside the
        // TabListCoordinator's SelectableListLayout.
        mSnackbarManager = new SnackbarManager(activity, rootView, null);
    }

    /** Destroys the tab list editor. */
    public void destroy() {
        if (mTabListEditorCoordinator != null) {
            mTabListEditorCoordinator.destroy();
        }
    }

    /** Initializes the tab list editor. */
    public void initTabListEditor() {
        // TODO(crbug.com/1504606): Permit a method of switching between selectable and closable
        // modes (or create separate instances).
        if (mTabListEditorCoordinator == null) {
            mTabListEditorCoordinator =
                    new TabListEditorCoordinator(
                            mActivity,
                            mCoordinatorView,
                            mBrowserControlsStateProvider,
                            mCurrentTabModelFilterSupplier,
                            mRegularTabModelSupplier,
                            mTabContentManager,
                            mTabListCoordinator::setRecyclerViewPosition,
                            mMode,
                            mRootView,
                            /* displayGroups= */ true,
                            mSnackbarManager,
                            TabProperties.UiType.SELECTABLE);
            mControllerSupplier.set(mTabListEditorCoordinator.getController());
        }
    }

    /** Shows the tab list editor with the default list of actions. */
    public void showTabListEditor() {
        initTabListEditor();
        if (mTabListEditorActions == null) {
            mTabListEditorActions = new ArrayList<>();
            mTabListEditorActions.add(
                    TabListEditorSelectionAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.END));
            mTabListEditorActions.add(
                    TabListEditorCloseAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
            mTabListEditorActions.add(
                    TabListEditorGroupAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
            mTabListEditorActions.add(
                    TabListEditorBookmarkAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
            mTabListEditorActions.add(
                    TabListEditorShareAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
        }

        var controller = mControllerSupplier.get();
        controller.configureToolbarWithMenuItems(
                mTabListEditorActions, new TabListEditorNavigationProvider(mActivity, controller));

        List<Tab> tabs = new ArrayList<>();
        TabList list = mCurrentTabModelFilterSupplier.get();
        for (int i = 0; i < list.getCount(); i++) {
            tabs.add(list.getTabAt(i));
        }
        controller.show(
                tabs, /* preSelectedTabCount= */ 0, mTabListCoordinator.getRecyclerViewPosition());
        TabUiMetricsHelper.recordSelectionEditorOpenMetrics(
                TabListEditorOpenMetricGroups.OPEN_FROM_GRID, mActivity);
    }

    /** Returns a supplier for {@link TabListEditorController}. */
    public ObservableSupplier<TabListEditorController> getControllerSupplier() {
        return mControllerSupplier;
    }
}
