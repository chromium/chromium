// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.SystemUiScrimDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;

/** Holds dependencies for constructing a {@link TabSwitcherPane}. */
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
    private final ScrimCoordinator mScrimCoordinator;
    private final SnackbarManager mSnackbarManager;
    private final ModalDialogManager mModalDialogManager;
    private final @TabListMode int mMode;
    private final @NonNull BottomSheetController mBottomSheetController;
    private final DataSharingTabManager mDataSharingTabManager;
    private final @NonNull BackPressManager mBackPressManager;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;

    private @Nullable TabSwitcherMessageManager mMessageManager;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param lifecycleDispatcher The lifecycle dispatcher for the activity.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabModelSelector For access to {@link TabModel}.
     * @param tabContentManager For management of thumbnails.
     * @param tabCreatorManager For creating new tabs.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param multiWindowModeStateDispatcher For managing behavior in multi-window.
     * @param rootUiScrimCoordinator The root UI coordinator's scrim coordinator. On LFF this is
     *     unused as the root UI's scrim coordinator is used for the show/hide animation.
     * @param snackbarManager The activity level snackbar manager.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param backPressManager Manages the different back press handlers throughout the app.
     * @param desktopWindowStateProvider Provider to get desktop window and app header state.
     */
    TabSwitcherPaneCoordinatorFactory(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator rootUiScrimCoordinator,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            @NonNull BackPressManager backPressManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileProviderSupplier = profileProviderSupplier;
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mTabCreatorManager = tabCreatorManager;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mScrimCoordinator =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                        ? createScrimCoordinatorForTablet(activity)
                        : rootUiScrimCoordinator;
        mSnackbarManager = snackbarManager;
        mModalDialogManager = modalDialogManager;
        mBottomSheetController = bottomSheetController;
        mDataSharingTabManager = dataSharingTabManager;
        mMode =
                TabUiFeatureUtilities.shouldUseListMode()
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID;
        mBackPressManager = backPressManager;
        mDesktopWindowStateProvider = desktopWindowStateProvider;
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
     * @return a {@link TabSwitcherPaneCoordinator} to use.
     */
    TabSwitcherPaneCoordinator create(
            @NonNull ViewGroup parentView,
            @NonNull TabSwitcherResetHandler resetHandler,
            @NonNull ObservableSupplier<Boolean> isVisibleSupplier,
            @NonNull ObservableSupplier<Boolean> isAnimatingSupplier,
            @NonNull Callback<Integer> onTabClickCallback,
            @NonNull Callback<Boolean> setHairlineVisibilityCallback,
            boolean isIncognito,
            @Nullable Runnable onTabGroupCreation,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        int token = mMessageManagerTokenHolder.acquireToken();
        assert mMessageManager != null;
        return new TabSwitcherPaneCoordinator(
                mActivity,
                mProfileProviderSupplier,
                createTabModelFilterSupplier(isIncognito),
                mTabContentManager,
                mTabCreatorManager,
                mBrowserControlsStateProvider,
                mScrimCoordinator,
                mModalDialogManager,
                mBottomSheetController,
                mDataSharingTabManager,
                mMessageManager,
                parentView,
                resetHandler,
                isVisibleSupplier,
                isAnimatingSupplier,
                onTabClickCallback,
                setHairlineVisibilityCallback,
                mMode,
                /* supportsEmptyState= */ !isIncognito,
                onTabGroupCreation,
                () -> mMessageManagerTokenHolder.releaseToken(token),
                edgeToEdgeSupplier,
                mDesktopWindowStateProvider);
    }

    /** Returns the {@link TabListMode} of the produced {@link TabListCoordinator}s. */
    @TabListMode
    int getTabListMode() {
        // This value will be determined at initialization time based on whether the device is
        // low-end and is not subject to change. Certain behaviors are limited to LIST vs GRID
        // mode and this information may be required even if a coordinator does not exist.
        return mMode;
    }

    /** Returns a scrim coordinator to use for tab grid dialog on LFF devices. */
    @VisibleForTesting
    static ScrimCoordinator createScrimCoordinatorForTablet(Activity activity) {
        ViewGroup coordinator = activity.findViewById(R.id.coordinator);
        // TODO(crbug.com/40067282): Because the show/hide animation already uses the
        // RootUiCoordinator's
        // ScrimCoordinator, a separate instance is needed. However, the way this is implemented the
        // status bar color is not updated. This should be fixed.
        SystemUiScrimDelegate delegate =
                new SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {}

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                };
        return new ScrimCoordinator(
                activity,
                delegate,
                coordinator,
                activity.getColor(R.color.omnibox_focused_fading_background_color));
    }

    @VisibleForTesting
    ObservableSupplier<TabModelFilter> createTabModelFilterSupplier(boolean isIncognito) {
        ObservableSupplierImpl<TabModelFilter> tabModelFilterSupplier =
                new ObservableSupplierImpl<>();
        // This implementation doesn't wait for isTabStateInitialized because we want to be able to
        // show the TabSwitcherPane before tab state initialization finishes. Tab state
        // initialization is an async process; when tab state restoration completes
        // TabModelObserver#restoreCompleted() is called which is listened for in
        // TabSwitcherPaneMediator to properly refresh the list in the event the contents changed.
        TabModelSelector selector = mTabModelSelector;
        if (!selector.getModels().isEmpty()) {
            tabModelFilterSupplier.set(
                    selector.getTabModelFilterProvider().getTabModelFilter(isIncognito));
        } else {
            selector.addObserver(
                    new TabModelSelectorObserver() {
                        @Override
                        public void onChange() {
                            assert !selector.getModels().isEmpty();
                            TabModelFilter filter =
                                    selector.getTabModelFilterProvider()
                                            .getTabModelFilter(isIncognito);
                            assert filter != null;
                            selector.removeObserver(this);
                            tabModelFilterSupplier.set(filter);
                        }
                    });
        }
        return tabModelFilterSupplier;
    }

    private void onMessageManagerTokenStateChanged() {
        if (mMessageManagerTokenHolder.hasTokens()) {
            assert mMessageManager == null : "MessageManager should not exist yet.";
            mMessageManager =
                    new TabSwitcherMessageManager(
                            mActivity,
                            mLifecycleDispatcher,
                            mTabModelSelector
                                    .getTabModelFilterProvider()
                                    .getCurrentTabModelFilterSupplier(),
                            mMultiWindowModeStateDispatcher,
                            mSnackbarManager,
                            mModalDialogManager,
                            mBrowserControlsStateProvider,
                            mTabContentManager,
                            mMode,
                            mActivity.findViewById(R.id.coordinator),
                            mTabCreatorManager.getTabCreator(/* incognito= */ false),
                            mBackPressManager,
                            mDesktopWindowStateProvider);
            if (mLifecycleDispatcher.isNativeInitializationFinished()) {
                mMessageManager.initWithNative(
                        mProfileProviderSupplier.get().getOriginalProfile(), getTabListMode());
            } else {
                mLifecycleDispatcher.register(
                        new NativeInitObserver() {
                            @Override
                            public void onFinishNativeInitialization() {
                                mMessageManager.initWithNative(
                                        mProfileProviderSupplier.get().getOriginalProfile(),
                                        getTabListMode());
                                mLifecycleDispatcher.unregister(this);
                            }
                        });
            }
        } else {
            mMessageManager.destroy();
            mMessageManager = null;
        }
    }

    TabSwitcherMessageManager getMessageManagerForTesting() {
        return mMessageManager;
    }
}
