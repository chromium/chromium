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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorOpenMetricGroups;
import org.chromium.chrome.browser.tinker_tank.TinkerTankDelegate;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages the {@link TabListEditorCoordinator} and related components for a {@link TabSwitcher}.
 */
public class TabListEditorManager {
    private final @NonNull Activity mActivity;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull ViewGroup mCoordinatorView;
    private final @NonNull ViewGroup mRootView;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final @NonNull TabContentManager mTabContentManager;
    private final @NonNull TabListCoordinator mTabListCoordinator;
    private final @TabListMode int mMode;
    private final @NonNull ObservableSupplierImpl<TabListEditorController> mControllerSupplier =
            new ObservableSupplierImpl<>();
    private final TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;
    private final @NonNull ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;

    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;
    private @Nullable List<TabListEditorAction> mTabListEditorActions;

    /**
     * @param activity The current activity.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param coordinatorView The overlay view to attach the editor to.
     * @param rootView The root view to attach the snackbar to.
     * @param browserControlsStateProvider The browser controls state provider.
     * @param currentTabModelFilterSupplier The supplier of the current {@link TabModelFilter}.
     * @param tabContentManager The {@link TabContentManager} for thumbnails.
     * @param tabListCoordinator The parent {@link TabListCoordinator}.
     * @param mode The {@link TabListMode} of the tab list (grid, list, etc.).
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    public TabListEditorManager(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ViewGroup coordinatorView,
            @NonNull ViewGroup rootView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabListCoordinator tabListCoordinator,
            BottomSheetController bottomSheetController,
            @TabListMode int mode,
            @Nullable Runnable onTabGroupCreation,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mCoordinatorView = coordinatorView;
        mRootView = rootView;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mTabListCoordinator = tabListCoordinator;
        mBottomSheetController = bottomSheetController;
        mMode = mode;
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(activity, modalDialogManager, onTabGroupCreation);
        mDesktopWindowStateProvider = desktopWindowStateProvider;

        // The snackbarManager used by mTabListEditorCoordinator. The rootView is the default
        // default parent view of the snackbar. When shown this will be re-parented inside the
        // TabListCoordinator's SelectableListLayout.
        if (!activity.isDestroyed() && !activity.isFinishing()) {
            mSnackbarManager = new SnackbarManager(activity, rootView, null);
        } else {
            mSnackbarManager = null;
        }
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
    }

    /** Destroys the tab list editor. */
    public void destroy() {
        if (mTabListEditorCoordinator != null) {
            mTabListEditorCoordinator.destroy();
        }
    }

    /** Initializes the tab list editor. */
    public void initTabListEditor() {
        // TODO(crbug.com/40945154): Permit a method of switching between selectable and closable
        // modes (or create separate instances).
        if (mTabListEditorCoordinator == null) {
            assert mSnackbarManager != null
                    : "SnackbarManager should have been created or the activity was already"
                            + " finishing.";
            mTabListEditorCoordinator =
                    new TabListEditorCoordinator(
                            mActivity,
                            mCoordinatorView,
                            mCoordinatorView,
                            mBrowserControlsStateProvider,
                            mCurrentTabModelFilterSupplier,
                            mTabContentManager,
                            mTabListCoordinator::setRecyclerViewPosition,
                            mMode,
                            /* displayGroups= */ true,
                            mSnackbarManager,
                            mBottomSheetController,
                            TabProperties.TabActionState.SELECTABLE,
                            /* gridCardOnClickListenerProvider= */ null,
                            mModalDialogManager,
                            mDesktopWindowStateProvider,
                            mEdgeToEdgeSupplier);
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
                            IconPosition.START,
                            /* actionConfirmationManager= */ null));
            mTabListEditorActions.add(
                    TabListEditorGroupAction.createAction(
                            mActivity,
                            mTabGroupCreationDialogManager,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
            mTabListEditorActions.add(
                    TabListEditorBookmarkAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
            if (TinkerTankDelegate.isEnabled()) {
                mTabListEditorActions.add(
                        TabListEditorTinkerTankAction.createAction(
                                mActivity,
                                ShowMode.MENU_ONLY,
                                ButtonType.ICON_AND_TEXT,
                                IconPosition.START));
            }
            mTabListEditorActions.add(
                    TabListEditorShareAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
        }

        var controller = mControllerSupplier.get();
        controller.show(
                TabModelUtils.convertTabListToListOfTabs(mCurrentTabModelFilterSupplier.get()),
                mTabListCoordinator.getRecyclerViewPosition());
        controller.configureToolbarWithMenuItems(mTabListEditorActions);

        TabUiMetricsHelper.recordSelectionEditorOpenMetrics(
                TabListEditorOpenMetricGroups.OPEN_FROM_GRID, mActivity);
    }

    /** Returns a supplier for {@link TabListEditorController}. */
    public ObservableSupplier<TabListEditorController> getControllerSupplier() {
        return mControllerSupplier;
    }
}
