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
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.HeadlessBrowserControlsStateProvider;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxTabUtils;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionService;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.ItemPickerSelectionHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
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
    private final Set<TabListEditorItemSelectionId> mInitialSelectedTabIds = new HashSet<>();
    private final int mAllowedSelectionCount;
    private final boolean mIsSingleContextMode;
    private final Set<Integer> mCachedTabIdsSet = new HashSet<>();
    private @Nullable Callback<Boolean> mSuccessCallback;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private @Nullable IncognitoTabModelObserver mIncognitoTabModelObserver;
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
            int allowedSelectionCount,
            boolean isSingleContextMode) {

        mProfileSupplier = profileSupplier;
        mWindowId = windowId;
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mRootView = rootView;
        mContainerView = containerView;
        mPreselectedTabIds = preselectedTabIds;
        mAllowedSelectionCount = allowedSelectionCount;
        mIsSingleContextMode = isSingleContextMode;

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

    private void runSuccessCallbackIfSame(boolean success, Callback<Boolean> successCallback) {
        if (mSuccessCallback == successCallback) {
            runSuccessCallback(success);
        }
    }

    private void runSuccessCallback(boolean success) {
        if (mSuccessCallback != null) {
            mSuccessCallback.onResult(success);
            mSuccessCallback = null;
        }
    }

    /**
     * Shows the Tab Item Picker UI.
     *
     * @param successCallback invoked with a false value if the picker cannot be shown or true
     *     otherwise.
     */
    void showTabItemPicker(Callback<Boolean> successCallback) {
        runSuccessCallback(false);
        mSuccessCallback = successCallback;

        mProfileSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (profile) -> {
                            if (mWindowId == TabWindowManager.INVALID_WINDOW_ID) {
                                runSuccessCallbackIfSame(false, successCallback);
                                return;
                            }
                            showTabItemPickerWithProfile(profile, mWindowId, successCallback);
                        }));
    }

    /**
     * Requests and initializes the TabModelSelector instance for a specific window using {@link
     * TabWindowManagerSingleton#requestSelectorWithoutActivity()} to access the list of tabs
     * without requiring a live {@code ChromeTabbedActivity}.
     *
     * @param profile The Profile instance required to scope the tab data.
     * @param windowId The ID of the Chrome window to load the selector for. This ID is used by
     *     {@code requestSelectorWithoutActivity()} to ensure the tab model is loaded and usable,
     *     with or without an activity holding the tab model loaded
     * @param successCallback The callback to execute once the tab model is available and the UI
     *     will show.
     */
    private void showTabItemPickerWithProfile(
            Profile profile, int windowId, Callback<Boolean> successCallback) {

        // Request the headless TabModelSelector instance.
        mTabModelSelector =
                TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(windowId, profile);

        if (mTabModelSelector == null) {
            runSuccessCallbackIfSame(false, successCallback);
            return;
        }

        // Wait for tab data (state from disk) to be fully initialized.
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (@Nullable TabModelSelector s) -> {
                            Profile currentProfile = mProfileSupplier.get();
                            boolean loaded = s != null && currentProfile != null;
                            runSuccessCallbackIfSame(loaded, successCallback);
                            if (loaded) {
                                assumeNonNull(s);
                                mTabListEditorCoordinator = createTabListEditorCoordinator(s);

                                assumeNonNull(currentProfile);
                                showTabsOnInitalLoad(currentProfile, s);
                            }
                        }));
    }

    /** Fetches the list of tabs to be displayed and shows the editor UI. */
    private void showTabsOnInitalLoad(Profile profile, TabModelSelector tabModelSelector) {
        // TODO(crbug.com/457858995): Use common tab filters.

        // Observe for model destruction as we shouldn't keep the picker around if the model it is
        // bound to is destroyed.
        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onDestroyed() {
                        cancelPicker(mActivity);
                        tabModelSelector.removeObserver(this);
                    }
                };
        tabModelSelector.addObserver(mTabModelSelectorObserver);
        if (profile.isIncognitoBranded()) {
            var incognitoTabModel =
                    (IncognitoTabModel) tabModelSelector.getModel(/* incognito= */ true);
            mIncognitoTabModelObserver =
                    new IncognitoTabModelObserver() {
                        @Override
                        public void didBecomeEmpty() {
                            cancelPicker(mActivity);
                            incognitoTabModel.removeIncognitoObserver(this);
                        }
                    };
            incognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);

            // Early out as incognito tabs are not cached.
            onCachedTabIdsRetrieved(new long[0]);
            return;
        }

        PageContentExtractionService pageContentExtractionService =
                PageContentExtractionServiceFactory.getForProfile(profile);
        pageContentExtractionService.getAllCachedTabIds(this::onCachedTabIdsRetrieved);
    }

    @VisibleForTesting
    void onCachedTabIdsRetrieved(long[] cachedTabIds) {
        if (mTabModelSelector == null) return;

        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            showEditorUi(new ArrayList<>());
            return;
        }

        TabModel tabModel = mTabModelSelector.getModel(profile.isIncognitoBranded());

        List<Tab> tabsToShow = new ArrayList<>();
        for (long id : cachedTabIds) {
            mCachedTabIdsSet.add((int) id);
        }

        int activeTabCount = 0;
        int cachedTabCount = 0;
        int backgroundTabCount = 0;
        // We cannot load background tabs in headless mode since the tabs are not attached to an
        // activity and thus cannot be loaded.
        boolean allowBackgroundTabContextCapture =
                ChromeFeatureList.sOnDemandBackgroundTabContextCapture.isEnabled()
                        && tabModel.getTabModelType() == TabModelType.STANDARD;
        for (Tab tab : tabModel) {
            // TODO(crbug.com/458152854): Allow reloading of tabs.
            boolean isActive = FuseboxTabUtils.isTabActive(tab);
            boolean isCached = mCachedTabIdsSet.contains(tab.getId());
            if (FuseboxTabUtils.isTabEligibleForAttachment(tab)
                    && (allowBackgroundTabContextCapture || isActive || isCached)) {
                tabsToShow.add(tab);
                if (isActive) activeTabCount++;
                if (isCached) cachedTabCount++;
                if (!isActive && !isCached) backgroundTabCount++;
            }
        }
        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.ActiveTabs.Count", activeTabCount);
        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.CachedTabs.Count", cachedTabCount);
        RecordHistogram.recordCount100Histogram(
                "Android.TabItemPicker.BackgroundTabs.Count", backgroundTabCount);
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

        controller
                .getHandleBackPressChangedSupplier()
                .addSyncObserverAndPostIfNonNull(mBackPressEnabledObserver);

        Tab currentTab = mTabModelSelector.getCurrentTab();
        int currentTabIndex = 0;
        if (currentTab != null) {
            int indexInFilteredList = tabs.indexOf(currentTab);
            if (indexInFilteredList != -1) {
                currentTabIndex = indexInFilteredList;
            }
        } else if (!tabs.isEmpty()) {
            // Find the last opened tab.
            Tab mostRecentTab = tabs.get(0);
            for (int i = 1; i < tabs.size(); i++) {
                Tab tab = tabs.get(i);
                // It is important to check if the tab is active, because we only want to scroll to
                // something that's been opened in our current session.
                if (tab.getTimestampMillis() > mostRecentTab.getTimestampMillis()
                        && FuseboxTabUtils.isTabActive(tab)) {
                    mostRecentTab = tab;
                }
            }
            currentTabIndex = tabs.indexOf(mostRecentTab);
        }
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
                var selectionId = TabListEditorItemSelectionId.createTabId(tab.getId());
                selectionSet.add(selectionId);
                mInitialSelectedTabIds.add(selectionId);
            }
        }
        controller.selectTabs(selectionSet);
    }

    private static void cancelPicker(Activity activity) {
        if (activity instanceof ChromeItemPickerActivity cipa) {
            cipa.finishWithCancel();
        } else {
            activity.finish();
        }
    }

    public static class ItemPickerNavigationProvider
            implements TabListEditorCoordinator.NavigationProvider, ItemPickerSelectionHandler {
        private final SettableNonNullObservableSupplier<Boolean> mEnableDoneButtonSupplier =
                ObservableSuppliers.createNonNull(false);
        private final Activity mActivity;
        private final MonotonicObservableSupplier<TabListEditorController> mControllerSupplier;
        private final TabModelSelector mTabModelSelector;
        private final Set<Integer> mCachedTabIds;
        private final Set<TabListEditorItemSelectionId> mInitialSelectedTabIds;

        public ItemPickerNavigationProvider(
                Activity activity,
                MonotonicObservableSupplier<TabListEditorController> controllerSupplier,
                TabModelSelector tabModelSelector,
                Set<Integer> cachedTabIds,
                Set<TabListEditorItemSelectionId> initialSelectedTabIds) {
            mActivity = activity;
            mControllerSupplier = controllerSupplier;
            mTabModelSelector = tabModelSelector;
            mCachedTabIds = cachedTabIds;
            mInitialSelectedTabIds = initialSelectedTabIds;
        }

        @Override
        public void onSelectionStateChange(Set<TabListEditorItemSelectionId> selectedItems) {
            boolean hasSelectionChanged = !Objects.equals(mInitialSelectedTabIds, selectedItems);
            mEnableDoneButtonSupplier.set(hasSelectionChanged);

            if (ChromeFeatureList.sOnDemandBackgroundTabContextCapture.isEnabled()) {
                // The maximum number of tabs that can be selected is determined by
                // mAllowedSelectionCount, which should always be sufficiently small that there is
                // no point caching which tabs have already been loaded. It is also safer to update
                // each time as the OS may kill background tabs at any time.
                for (TabListEditorItemSelectionId item : selectedItems) {
                    assert item.isTabId();
                    int tabId = item.getTabId();

                    if (mCachedTabIds.contains(tabId)) continue;

                    Tab tab = mTabModelSelector.getTabById(tabId);
                    if (tab == null
                            || !FuseboxTabUtils.isTabEligibleForAttachment(tab)
                            || FuseboxTabUtils.isTabActive(tab)) {
                        continue;
                    }

                    // If everything is working as expected the current tab should always be active
                    // and therefore not loaded on demand, but just in case we still allow it to be
                    // loaded here.
                    tab.loadIfNeeded(TabLoadIfNeededCaller.FUSEBOX_ATTACHMENT);

                    // TODO(crbug.com/486943788): On load complete, capture a new thumbnail and try
                    // to extract the context. The context extraction might be left to
                    // FuseboxAttachment, this is still under investigation.
                }
            }
        }

        @Override
        public NonNullObservableSupplier<Boolean> getEnableDoneButtonSupplier() {
            return mEnableDoneButtonSupplier;
        }

        @Override
        public void goBack() {
            var controller = mControllerSupplier.get();
            assert controller != null;
            if (controller.isVisible()) {
                controller.hide();
            }

            // Route back press to the Activity's cancel handler.
            cancelPicker(mActivity);
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
            var controller = mControllerSupplier.get();
            assert controller != null;
            controller.hideByAction();

            // Route the result to the Activity's success handler.
            if (mActivity instanceof ChromeItemPickerActivity cipa) {
                cipa.finishWithSelectedItems(selectedItems);
            } else {
                mActivity.finish();
            }
        }
    }

    /** Creates a TabGroupModelFilter instance required by the TabListEditorCoordinator. */
    private NullableObservableSupplier<TabGroupModelFilter> createTabGroupModelFilterSupplier(
            TabModelSelector tabModelSelector) {
        boolean isIncognito = assumeNonNull(mProfileSupplier.get()).isIncognitoBranded();
        TabGroupModelFilter curFilter = tabModelSelector.getTabGroupModelFilter(isIncognito);
        return curFilter == null
                ? ObservableSuppliers.alwaysNull()
                : ObservableSuppliers.createNonNull(curFilter);
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
        if (mTabModelSelector != null && mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            if (mIncognitoTabModelObserver != null
                    && mTabModelSelector.getModel(true)
                            instanceof IncognitoTabModel incognitoTabModel) {
                incognitoTabModel.removeIncognitoObserver(mIncognitoTabModelObserver);
            }
        }
        runSuccessCallback(false);
        mBackPressCallback.remove();
        mCallbackController.destroy();
    }

    /** Creates a TabListEditorCoordinator with set configurations for the Tab Picker UI. */
    @VisibleForTesting
    TabListEditorCoordinator createTabListEditorCoordinator(TabModelSelector selector) {
        NullableObservableSupplier<TabGroupModelFilter> tabGroupModelFilterSupplier =
                createTabGroupModelFilterSupplier(selector);
        BrowserControlsStateProvider browserControlStateProvider =
                new HeadlessBrowserControlsStateProvider();
        TabContentManager tabContentManager =
                createTabContentManager(selector, browserControlStateProvider);
        ModalDialogManager modalDialogManager =
                new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP);

        SettableMonotonicObservableSupplier<TabListEditorController> controllerSupplier =
                ObservableSuppliers.createMonotonic();
        mNavigationProvider =
                new ItemPickerNavigationProvider(
                        mActivity,
                        controllerSupplier,
                        assumeNonNull(mTabModelSelector),
                        mCachedTabIdsSet,
                        mInitialSelectedTabIds);

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
                        mNavigationProvider,
                        /* undoBarExplicitTrigger= */ null,
                        /* componentName= */ "TabItemPickerCoordinator",
                        mAllowedSelectionCount,
                        mIsSingleContextMode);

        controllerSupplier.set(coordinator.getController());
        coordinator.getController().setNavigationProvider(mNavigationProvider);

        return coordinator;
    }

    public @Nullable ItemPickerNavigationProvider getItemPickerNavigationProviderForTesting() {
        return mNavigationProvider;
    }
}
