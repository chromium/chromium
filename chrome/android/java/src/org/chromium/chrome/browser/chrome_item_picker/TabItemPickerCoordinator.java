// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.HeadlessBrowserControlsStateProvider;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.Collections;
import java.util.List;

/** Provides access to, and management of, Tab data for the {@link ChromeItemPickerActivity}. */
@NullMarked
public class TabItemPickerCoordinator {
    private final int mWindowId;
    private final OneshotSupplier<Profile> mProfileSupplier;
    private final CallbackController mCallbackController;
    private final Activity mActivity;
    private final ViewGroup mRootView;
    private final ViewGroup mContainerView;
    private final SnackbarManager mSnackbarManager;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;

    public TabItemPickerCoordinator(
            OneshotSupplier<Profile> profileSupplier,
            int windowId,
            Activity activity,
            SnackbarManager snackbarManager,
            ViewGroup rootView,
            ViewGroup containerView) {

        mProfileSupplier = profileSupplier;
        mWindowId = windowId;
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mRootView = rootView;
        mContainerView = containerView;

        mCallbackController = new CallbackController();
    }

    /**
     * Initiates the asynchronous loading of the TabModel data and calls the core logic to acquire
     * the TabModelSelector.
     *
     * @param callback The callback to execute once the TabModelSelector is fully initialized (or
     *     null).
     */
    // TODO: Return selected tabs instead of the TabModelSelector
    void showTabItemPicker(Callback<@Nullable TabModelSelector> callback) {
        mProfileSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (profile) -> {
                            if (mWindowId == TabWindowManager.INVALID_WINDOW_ID) {
                                callback.onResult(null);
                                return;
                            }
                            showTabItemPickerWithProfile(profile, mWindowId, callback);
                        }));
    }

    /**
     * Requests and initializes the headless TabModelSelector instance for a specific window using
     * {@link TabWindowManagerSingleton#requestSelectorWithoutActivity()}to access the list of tabs
     * without requiring a live {@code ChromeTabbedActivity}.
     *
     * @param profile The Profile instance required to scope the tab data.
     * @param windowId The ID of the Chrome window to load the selector for. This ID is used by
     *     {@code requestSelectorWithoutActivity()} to ensure the tab model is loaded and usable,
     *     with or without an activity holding the tab model loaded
     * @param callback The callback to execute once the TabModelSelector is fully initialized.
     */
    private void showTabItemPickerWithProfile(
            Profile profile, int windowId, Callback<@Nullable TabModelSelector> callback) {

        // Request the headless TabModelSelector instance.
        mTabModelSelector =
                TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(windowId, profile);

        if (mTabModelSelector == null) {
            callback.onResult(null);
            return;
        }

        // Wait for tab data (state from disk) to be fully initialized.
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (@Nullable TabModelSelector s) -> {
                            callback.onResult(s);
                            if (s != null) {
                                onTabModelReadyForUi(s);
                            }
                        }));
    }

    private void onTabModelReadyForUi(TabModelSelector selector) {
        mTabListEditorCoordinator = createTabListEditorCoordinator(selector);
        TabListEditorController controller = mTabListEditorCoordinator.getController();
        List<Tab> allTabs = TabModelUtils.convertTabListToListOfTabs(selector.getModel(false));

        int currentTabIndex = selector.getModel(/* incognito= */ false).index();
        RecyclerViewPosition position = new RecyclerViewPosition(currentTabIndex, 0);

        controller.show(
                allTabs,
                /* tabGroupSyncIds= */ Collections.emptyList(),
                /* recyclerViewPosition= */ position);
    }

    /** Creates a TabListEditorCoordinator with set configurations for the Tab Picker UI. */
    @VisibleForTesting
    TabListEditorCoordinator createTabListEditorCoordinator(TabModelSelector selector) {
        ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier =
                createTabGroupModelFilterSupplier(selector);
        BrowserControlsStateProvider browserControlStateProvider =
                new HeadlessBrowserControlsStateProvider();
        TabContentManager tabContentManager =
                createTabContentManager(selector, browserControlStateProvider);
        ModalDialogManager modalDialogManager =
                new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP);

        // TODO: Create a gridCardOnClickListenerProvider that overrides the default behavior to
        // observe the tabs selected.
        return new TabListEditorCoordinator(
                mActivity,
                mRootView,
                mContainerView,
                browserControlStateProvider,
                tabGroupModelFilterSupplier,
                tabContentManager,
                CallbackUtils.emptyCallback(),
                TabListMode.GRID,
                /* displayGroups= */ false,
                mSnackbarManager,
                /* bottomSheetController= */ null,
                TabProperties.TabActionState.SELECTABLE,
                /* gridCardOnClickListenerProvider= */ null,
                modalDialogManager,
                /* desktopWindowStateManager= */ null,
                /* edgeToEdgeSupplier= */ null,
                CreationMode.FULL_SCREEN,
                /* undoBarExplicitTrigger= */ null,
                /* componentName= */ "TabItemPickerCoordinator");
    }

    /** Creates a TabGroupModelFilter instance required by the TabListEditorCoordinator. */
    private ObservableSupplier<@Nullable TabGroupModelFilter> createTabGroupModelFilterSupplier(
            TabModelSelector tabModelSelector) {
        return new ObservableSupplierImpl<@Nullable TabGroupModelFilter>(
                tabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false));
    }

    /** Creates a TabContentManager instance required by the TabListEditorCoordinator. */
    private TabContentManager createTabContentManager(
            TabModelSelector selector, BrowserControlsStateProvider browserControlsStateProvider) {
        return new TabContentManager(
                mActivity,
                browserControlsStateProvider,
                /* snapshotsEnabled= */ true,
                selector::getTabById,
                TabWindowManagerSingleton.getInstance());
    }

    /** Cleans up the TabListEditorCoordinator and releases resources. */
    public void destroy() {
        if (mTabListEditorCoordinator != null) {
            mTabListEditorCoordinator.destroy();
        }
        mCallbackController.destroy();
    }
}
