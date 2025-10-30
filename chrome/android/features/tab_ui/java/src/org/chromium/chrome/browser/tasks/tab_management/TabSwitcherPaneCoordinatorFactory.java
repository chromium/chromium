// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;

import java.util.function.Supplier;

/** Holds dependencies for constructing a {@link TabSwitcherPane}. */
@NullMarked
public class TabSwitcherPaneCoordinatorFactory {
    private final TokenHolder mMessageManagerTokenHolder =
            new TokenHolder(this::onMessageManagerTokenStateChanged);

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final TabModelSelector mTabModelSelector;
    private final TabContentManager mTabContentManager;
    private final TabCreatorManager mTabCreatorManager;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final ScrimManager mScrimManager;
    private final SnackbarManager mSnackbarManager;
    private final ModalDialogManager mModalDialogManager;
    private final @TabListMode int mMode;
    private final BottomSheetController mBottomSheetController;
    private final DataSharingTabManager mDataSharingTabManager;
    private final BackPressManager mBackPressManager;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final UndoBarThrottle mUndoBarThrottle;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private @Nullable TabSwitcherMessageManager mMessageManager;
    private final @Nullable TabSwitcherDragHandler mTabSwitcherDragHandler;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param lifecycleDispatcher The lifecycle dispatcher for the activity.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabModelSelector For access to {@link TabModel}.
     * @param tabContentManager For management of thumbnails.
     * @param tabCreatorManager For creating new tabs.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param multiWindowModeStateDispatcher For managing behavior in multi-window.
     * @param scrimManager Root scrim manager.
     * @param snackbarManager The activity level snackbar manager.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param backPressManager Manages the different back press handlers throughout the app.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param undoBarThrottle Used to throttle the undo bar.
     * @param paneManagerSupplier Used to switch and communicate with other panes.
     * @param tabGroupUiActionHandlerSupplier Used to open hidden tab groups.
     * @param layoutStateProviderSupplier Supplies the LayoutStateProvider, which is used to observe
     *     when the TabSwitcher is hidden.
     * @param tabSwitcherDragHandler An instance of the {@link TabSwitcherDragHandler}.
     */
    TabSwitcherPaneCoordinatorFactory(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ScrimManager scrimManager,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            BackPressManager backPressManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            UndoBarThrottle undoBarThrottle,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier,
            @Nullable TabSwitcherDragHandler tabSwitcherDragHandler) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileProviderSupplier = profileProviderSupplier;
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mTabCreatorManager = tabCreatorManager;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mScrimManager = scrimManager;
        mSnackbarManager = snackbarManager;
        mModalDialogManager = modalDialogManager;
        mBottomSheetController = bottomSheetController;
        mDataSharingTabManager = dataSharingTabManager;
        mMode = TabListCoordinator.TabListMode.GRID;
        mBackPressManager = backPressManager;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mUndoBarThrottle = undoBarThrottle;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mTabSwitcherDragHandler = tabSwitcherDragHandler;
    }

    /**
     * Creates a {@link TabSwitcherPaneCoordinator} using a combination of the held dependencies and
     * those supplied through this method.
     *
     * @param parentView The view to use as a parent.
     * @param resetHandler The reset handler to drive updates.
     * @param isVisibleSupplier Supplies visibility information to the tab switcher.
     * @param isAnimatingSupplier Supplies animation information to the tab switcher.
     * @param onTabClickCallback Callback to be invoked with the tab ID of the selected tab.
     * @param setHairlineVisibilityCallback Callback to be invoked to show or hide the hairline.
     * @param isIncognito Whether this is for the incognito tab switcher.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param setOverlayViewCallback Sets a view to overlay the Hub.
     * @param hubSearchBoxVisibilitySupplier Used to set the visibility of the hub search box.
     * @return a {@link TabSwitcherPaneCoordinator} to use.
     */
    TabSwitcherPaneCoordinator create(
            ViewGroup parentView,
            TabSwitcherResetHandler resetHandler,
            ObservableSupplier<Boolean> isVisibleSupplier,
            ObservableSupplier<Boolean> isAnimatingSupplier,
            Callback<Integer> onTabClickCallback,
            boolean isIncognito,
            @Nullable Runnable onTabGroupCreation,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            Callback<@Nullable View> setOverlayViewCallback,
            ObservableSupplierImpl<Boolean> hubSearchBoxVisibilitySupplier) {
        int token = mMessageManagerTokenHolder.acquireToken();
        assert mMessageManager != null;

        return new TabSwitcherPaneCoordinator(
                mActivity,
                assertNonNull(mProfileProviderSupplier.get()),
                createTabGroupModelFilterSupplier(isIncognito),
                mTabContentManager,
                mBrowserControlsStateProvider,
                mScrimManager,
                mModalDialogManager,
                mBottomSheetController,
                mDataSharingTabManager,
                mMessageManager,
                parentView,
                resetHandler,
                isVisibleSupplier,
                isAnimatingSupplier,
                onTabClickCallback,
                mMode,
                /* supportsEmptyState= */ !isIncognito,
                onTabGroupCreation,
                () -> mMessageManagerTokenHolder.releaseToken(token),
                edgeToEdgeSupplier,
                mDesktopWindowStateManager,
                mShareDelegateSupplier,
                mTabBookmarkerSupplier,
                mUndoBarThrottle,
                setOverlayViewCallback,
                mTabSwitcherDragHandler,
                hubSearchBoxVisibilitySupplier);
    }

    /** Returns the {@link TabListMode} of the produced {@link TabListCoordinator}s. */
    @TabListMode
    int getTabListMode() {
        // This value will be determined at initialization time based on whether the device is
        // low-end and is not subject to change. Certain behaviors are limited to LIST vs GRID
        // mode and this information may be required even if a coordinator does not exist.
        return mMode;
    }

    @VisibleForTesting
    ObservableSupplier<@Nullable TabGroupModelFilter> createTabGroupModelFilterSupplier(
            boolean isIncognito) {
        ObservableSupplierImpl<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier =
                new ObservableSupplierImpl<>();
        // This implementation doesn't wait for isTabStateInitialized because we want to be able to
        // show the TabSwitcherPane before tab state initialization finishes. Tab state
        // initialization is an async process; when tab state restoration completes
        // TabModelObserver#restoreCompleted() is called which is listened for in
        // TabSwitcherPaneMediator to properly refresh the list in the event the contents changed.
        TabModelSelector selector = mTabModelSelector;
        if (!selector.getModels().isEmpty()) {
            TabGroupModelFilter filter =
                    selector.getTabGroupModelFilterProvider().getTabGroupModelFilter(isIncognito);
            tabGroupModelFilterSupplier.set(filter);
        } else {
            selector.addObserver(
                    new TabModelSelectorObserver() {
                        @Override
                        public void onChange() {
                            assert !selector.getModels().isEmpty();
                            TabGroupModelFilter filter =
                                    selector.getTabGroupModelFilterProvider()
                                            .getTabGroupModelFilter(isIncognito);
                            assert filter != null;
                            selector.removeObserver(this);
                            tabGroupModelFilterSupplier.set(filter);
                        }
                    });
        }
        return tabGroupModelFilterSupplier;
    }

    private void onMessageManagerTokenStateChanged() {
        if (mMessageManagerTokenHolder.hasTokens()) {
            assert mMessageManager == null : "MessageManager should not exist yet.";
            mMessageManager =
                    new TabSwitcherMessageManager(
                            mActivity,
                            mLifecycleDispatcher,
                            mTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getCurrentTabGroupModelFilterSupplier(),
                            mMultiWindowModeStateDispatcher,
                            mSnackbarManager,
                            mModalDialogManager,
                            mBrowserControlsStateProvider,
                            mTabContentManager,
                            mMode,
                            mActivity.findViewById(R.id.coordinator),
                            mTabCreatorManager.getTabCreator(/* incognito= */ false),
                            mBackPressManager,
                            mDesktopWindowStateManager,
                            mEdgeToEdgeSupplier,
                            mPaneManagerSupplier,
                            mTabGroupUiActionHandlerSupplier,
                            mLayoutStateProviderSupplier);
            if (mLifecycleDispatcher.isNativeInitializationFinished()) {
                mMessageManager.initWithNative(
                        assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile(),
                        getTabListMode());
            } else {
                mLifecycleDispatcher.register(
                        new NativeInitObserver() {
                            @Override
                            public void onFinishNativeInitialization() {
                                if (mMessageManager != null) {
                                    mMessageManager.initWithNative(
                                            assumeNonNull(mProfileProviderSupplier.get())
                                                    .getOriginalProfile(),
                                            getTabListMode());
                                }
                                mLifecycleDispatcher.unregister(this);
                            }
                        });
            }
        } else {
            assumeNonNull(mMessageManager);
            mMessageManager.destroy();
            mMessageManager = null;
        }
    }

    @Nullable TabSwitcherMessageManager getMessageManagerForTesting() {
        return mMessageManager;
    }
}
