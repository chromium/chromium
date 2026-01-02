// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.activity.ComponentActivity;
import androidx.activity.OnBackPressedCallback;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.HeadlessBrowserControlsStateProvider;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxTabUtils;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionService;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionServiceFactory;
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
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

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
    private final OnBackPressedCallback mBackPressCallback;
    private final Callback<Boolean> mBackPressEnabledObserver;
    private final ArrayList<Integer> mPreselectedTabIds;
    private final int mAllowedSelectionCount;
    private final Set<Integer> mCachedTabIdsSet = new HashSet<>();
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;
    private @Nullable ItemPickerNavigationProvider mNavigationProvider;

    public TabItemPickerCoordinator(
            OneshotSupplier<Profile> profileSupplier,
            int windowId,
            Activity activity,
            SnackbarManager snackbarManager,
            ViewGroup rootView,
            ViewGroup containerView,
            ArrayList<Integer> preselectedTabIds,
            int allowedSelectionCount) {

        mProfileSupplier = profileSupplier;
        mWindowId = windowId;
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mRootView = rootView;
        mContainerView = containerView;
        mPreselectedTabIds = preselectedTabIds;
        mAllowedSelectionCount = allowedSelectionCount;

        mBackPressCallback =
                new OnBackPressedCallback(/* enabled= */ false) {
                    @Override
                    public void handleOnBackPressed() {
                        if (mTabListEditorCoordinator != null) {
                            mTabListEditorCoordinator.getController().handleBackPress();
                        }
                    }
                };
        mBackPressEnabledObserver = mBackPressCallback::setEnabled;
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
                                mTabListEditorCoordinator = createTabListEditorCoordinator(s);
                                refreshTabsToShow();
                            }
                        }));
    }

    /** Fetches the list of tabs to be displayed and shows the editor UI. */
    private void refreshTabsToShow() {
        // TODO(crbug.com/457858995): Use common tab filters.
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isIncognitoBranded()) {
            onCachedTabIdsRetrieved(new long[0]);
            return;
        }

        PageContentExtractionService pageContentExtractionService =
                PageContentExtractionServiceFactory.getForProfile(profile);
        pageContentExtractionService.getAllCachedTabIds(this::onCachedTabIdsRetrieved);
    }

    void onCachedTabIdsRetrieved(long[] cachedTabIds) {
        if (mTabModelSelector == null) return;

        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            showEditorUi(new ArrayList<>());
            return;
        }

        List<Tab> allTabs =
                TabModelUtils.convertTabListToListOfTabs(
                        mTabModelSelector.getModel(profile.isIncognitoBranded()));

        List<Tab> tabsToShow = new ArrayList<>();
        for (long id : cachedTabIds) {
            mCachedTabIdsSet.add((int) id);
        }

        int activeTabCount = 0;
        int cachedTabCount = 0;
        for (Tab tab : allTabs) {
            // TODO(crbug.com/458152854): Allow reloading of tabs.
            boolean isActive = FuseboxTabUtils.isTabActive(tab);
            boolean isCached = mCachedTabIdsSet.contains(tab.getId());
            if (FuseboxTabUtils.isTabEligibleForAttachment(tab) && (isActive || isCached)) {
                tabsToShow.add(tab);
                if (isActive) activeTabCount++;
                if (isCached) cachedTabCount++;
            }
        }
        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.ActiveTabs.Count", activeTabCount);
        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.CachedTabs.Count", cachedTabCount);
        showEditorUi(tabsToShow);
    }

    private void showEditorUi(List<Tab> tabs) {
        if (mTabListEditorCoordinator == null || mTabModelSelector == null) return;

        TabListEditorController controller = mTabListEditorCoordinator.getController();

        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.SelectableTabs.Count", tabs.size());

        if (mActivity instanceof ComponentActivity componentActivity) {
            // Add the callback to the Dispatcher
            componentActivity
                    .getOnBackPressedDispatcher()
                    .addCallback(componentActivity, mBackPressCallback);
        }

        controller.getHandleBackPressChangedSupplier().addObserver(mBackPressEnabledObserver);

        Profile profile = mProfileSupplier.get();
        int currentTabIndex =
                mTabModelSelector
                        .getModel(profile == null ? false : profile.isIncognitoBranded())
                        .index();
        RecyclerViewPosition position = new RecyclerViewPosition(currentTabIndex, 0);

        controller.show(
                tabs,
                /* tabGroupSyncIds= */ Collections.emptyList(),
                /* recyclerViewPosition= */ position);
        if (mPreselectedTabIds.isEmpty()) return;
        Set<TabListEditorItemSelectionId> selectionSet = new HashSet<>();
        for (Integer id : mPreselectedTabIds) {
            if (id == null) continue;
            @Nullable Tab tab = mTabModelSelector.getTabById(id);
            if (tab != null) {
                selectionSet.add(TabListEditorItemSelectionId.createTabId(tab.getId()));
            }
        }
        controller.preselectTabs(selectionSet);
    }

    public interface ItemPickerSelectionHandler {

        /**
         * Executes the successful selection logic, typically finishing the host activity. * @param
         * selectedItems The list of items chosen by the user.
         */
        void finishSelection(List<TabListEditorItemSelectionId> selectedItems);
    }

    public static class ItemPickerNavigationProvider
            implements TabListEditorCoordinator.NavigationProvider, ItemPickerSelectionHandler {
        private final Activity mActivity;
        private final TabListEditorController mController;
        private final TabModelSelector mTabModelSelector;
        private final Set<Integer> mCachedTabIds;

        public ItemPickerNavigationProvider(
                Activity activity,
                TabListEditorController controller,
                SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate,
                TabModelSelector tabModelSelector,
                Set<Integer> cachedTabIds) {
            mActivity = activity;
            mController = controller;
            mTabModelSelector = tabModelSelector;
            mCachedTabIds = cachedTabIds;
        }

        @Override
        public void goBack() {
            if (mController.isVisible()) {
                mController.hide();
            }

            // Route back press to the Activity's cancel handler.
            if (mActivity instanceof ChromeItemPickerActivity cipa) {
                cipa.finishWithCancel();
            } else {
                mActivity.finish();
            }
        }

        @Override
        public void finishSelection(List<TabListEditorItemSelectionId> selectedItems) {
            int activePickedCount = 0;
            int cachedPickedCount = 0;

            for (TabListEditorItemSelectionId item : selectedItems) {
                if (!item.isTabId()) continue;

                int tabId = item.getTabId();
                Tab tab = mTabModelSelector.getTabById(tabId);

                if (tab != null && FuseboxTabUtils.isTabActive(tab)) {
                    activePickedCount++;
                }
                if (mCachedTabIds.contains(tabId)) {
                    cachedPickedCount++;
                }
            }
            RecordHistogram.recordCount100Histogram(
                    "Android.TabItemPicker.SelectedTabs.Count", selectedItems.size());
            RecordHistogram.recordCount100Histogram(
                    "Android.TabItemPicker.ActiveTabsPicked.Count", activePickedCount);
            RecordHistogram.recordCount100Histogram(
                    "Android.TabItemPicker.CachedTabsPicked.Count", cachedPickedCount);
            mController.hideByAction();

            // Route the result to the Activity's success handler.
            if (mActivity instanceof ChromeItemPickerActivity cipa) {
                cipa.finishWithSelectedItems(selectedItems);
            } else {
                mActivity.finish();
            }
        }
    }

    /** Creates a TabGroupModelFilter instance required by the TabListEditorCoordinator. */
    private ObservableSupplier<@Nullable TabGroupModelFilter> createTabGroupModelFilterSupplier(
            TabModelSelector tabModelSelector) {
        boolean isIncognito = assumeNonNull(mProfileSupplier.get()).isIncognitoBranded();
        return new ObservableSupplierImpl<@Nullable TabGroupModelFilter>(
                tabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(isIncognito));
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
            mTabListEditorCoordinator
                    .getController()
                    .getHandleBackPressChangedSupplier()
                    .removeObserver(mBackPressEnabledObserver);
            mTabListEditorCoordinator.destroy();
        }
        mBackPressCallback.remove();
        mCallbackController.destroy();
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

        TabListEditorCoordinator coordinator =
                new TabListEditorCoordinator(
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
                        CreationMode.ITEM_PICKER,
                        /* undoBarExplicitTrigger= */ null,
                        /* componentName= */ "TabItemPickerCoordinator",
                        mAllowedSelectionCount);

        mNavigationProvider =
                new ItemPickerNavigationProvider(
                        mActivity,
                        coordinator.getController(),
                        coordinator.getSelectionDelegate(),
                        assumeNonNull(mTabModelSelector),
                        mCachedTabIdsSet);

        coordinator.getController().setNavigationProvider(mNavigationProvider);
        coordinator.getController().setSelectionHandler(mNavigationProvider);

        return coordinator;
    }

    public @Nullable ItemPickerNavigationProvider getItemPickerNavigationProviderForTesting() {
        return mNavigationProvider;
    }
}
