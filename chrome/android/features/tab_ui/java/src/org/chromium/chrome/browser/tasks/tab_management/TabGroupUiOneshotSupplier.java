// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A custom {@link OneshotSupplier} for a {@link TabGroupUi}. The supplied value will remain null
 * until the current activity tab is in a tab group.
 */
public class TabGroupUiOneshotSupplier extends OneshotSupplierImpl<TabGroupUi> {

    /** Controller containing the logic that manages when the supplier is set with a value. */
    private static class TabGroupUiCreationController {
        private final TabObserver mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onTabGroupIdChanged(Tab tab, @Nullable Token tabGroupId) {
                        postMaybeCreateTabGroupUi(tab);
                    }
                };
        private final ValueChangedCallback<Tab> mActivityTabObserver =
                new ValueChangedCallback<>(this::onActivityTabChanged);
        private final ActivityTabProvider mActivityTabProvider;
        private final TabModelSelector mTabModelSelector;
        private @Nullable Runnable mSetter;
        private @Nullable CallbackController mCallbackController = new CallbackController();

        TabGroupUiCreationController(
                ActivityTabProvider activityTabProvider,
                TabModelSelector tabModelSelector,
                Runnable setter) {
            mSetter = setter;
            mActivityTabProvider = activityTabProvider;
            mTabModelSelector = tabModelSelector;
            activityTabProvider.addObserver(mActivityTabObserver);
        }

        void destroy() {
            if (mCallbackController != null) {
                mCallbackController.destroy();
                mCallbackController = null;
            }
            mActivityTabProvider.removeObserver(mActivityTabObserver);
            // Trigger a null new tab selection to effectively unregister the observer from the old
            // tab.
            mActivityTabObserver.onResult(null);
        }

        private void postMaybeCreateTabGroupUi(Tab tab) {
            if (mCallbackController == null) return;

            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(() -> maybeCreateTabGroupUi(tab)));
        }

        private void maybeCreateTabGroupUi(Tab tab) {
            if (mSetter == null) return;

            if (tab == null || tab.isClosing() || tab.isDestroyed()) return;

            boolean isInTabGroup =
                    mTabModelSelector
                            .getTabModelFilterProvider()
                            .getTabModelFilter(tab.isIncognito())
                            .isTabInTabGroup(tab);
            if (!isInTabGroup) return;

            mSetter.run();
            mSetter = null;
        }

        private void onActivityTabChanged(@Nullable Tab newTab, @Nullable Tab oldTab) {
            if (oldTab != null) {
                oldTab.removeObserver(mTabObserver);
            }

            if (newTab != null) {
                postMaybeCreateTabGroupUi(newTab);

                newTab.addObserver(mTabObserver);
            }
        }
    }

    private @Nullable TabGroupUiCreationController mTabGroupUiCreationController;

    /**
     * Constructs a specialized {@link OneshotSupplier} for {@link TabGroupUi}.
     *
     * @param activityTabProvider Gives access to the current tab.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param activity The {@link Activity} that creates this surface.
     * @param parentView The parent view of this UI.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} of the top
     *     controls.
     * @param incognitoStateProvider Observable provider of incognito state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param tabContentManager Gives access to the tab content.
     * @param tabCreatorManager Manages creation of tabs.
     * @param layoutStateProviderSupplier Supplies the {@link LayoutStateProvider}.
     * @param modalDialogManager Used to show confirmation dialogs.
     */
    public TabGroupUiOneshotSupplier(
            ActivityTabProvider activityTabProvider,
            TabModelSelector tabModelSelector,
            Activity activity,
            ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            IncognitoStateProvider incognitoStateProvider,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ModalDialogManager modalDialogManager) {
        Runnable setter =
                () -> {
                    var tabGroupUi =
                            TabManagementDelegateProvider.getDelegate()
                                    .createTabGroupUi(
                                            activity,
                                            parentView,
                                            browserControlsStateProvider,
                                            incognitoStateProvider,
                                            scrimCoordinator,
                                            omniboxFocusStateSupplier,
                                            bottomSheetController,
                                            dataSharingTabManager,
                                            tabModelSelector,
                                            tabContentManager,
                                            tabCreatorManager,
                                            layoutStateProviderSupplier,
                                            modalDialogManager);
                    set(tabGroupUi);
                    maybeDestroyTabGroupUiCreationController();
                };
        mTabGroupUiCreationController =
                new TabGroupUiCreationController(activityTabProvider, tabModelSelector, setter);
    }

    /** Removes all callbacks and observers. */
    public void destroy() {
        maybeDestroyTabGroupUiCreationController();
    }

    private void maybeDestroyTabGroupUiCreationController() {
        if (mTabGroupUiCreationController != null) {
            mTabGroupUiCreationController.destroy();
            mTabGroupUiCreationController = null;
        }
    }
}
