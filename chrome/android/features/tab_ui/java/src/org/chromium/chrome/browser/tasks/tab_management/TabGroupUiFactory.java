// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Factory for creating {@link TabGroupUi}. */
@NullMarked
public class TabGroupUiFactory {
    private static @Nullable Factory sFactoryForTesting;

    @FunctionalInterface
    interface Factory {
        TabGroupUi createTabGroupUi(
                Activity activity,
                ViewGroup parentView,
                BrowserControlsStateProvider browserControlsStateProvider,
                ScrimManager scrimManager,
                NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
                BottomSheetController bottomSheetController,
                DataSharingTabManager dataSharingTabManager,
                TabModelSelector tabModelSelector,
                TabContentManager tabContentManager,
                TabCreatorManager tabCreatorManager,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                ModalDialogManager modalDialogManager,
                ThemeColorProvider themeColorProvider,
                UndoBarThrottle undoBarThrottle,
                MonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
                Supplier<ShareDelegate> shareDelegateSupplier);
    }

    private TabGroupUiFactory() {}

    /**
     * Creates a {@link TabGroupUi} instance.
     *
     * @param activity The {@link Activity} that creates this surface.
     * @param parentView The parent view of this UI.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} of the top
     *     controls.
     * @param scrimManager The {@link ScrimManager} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link DataSharingTabManager} managing communication between
     *     UI and DataSharing services.
     * @param tabModelSelector Gives access to the current set of {@link TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param tabCreatorManager Manages creation of tabs.
     * @param layoutStateProviderSupplier Supplies the {@link LayoutStateProvider}.
     * @param modalDialogManager Used to show confirmation dialogs.
     * @param themeColorProvider Used to provide the theme.
     * @param undoBarThrottle Used to suppress the undo bar.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @return The {@link TabGroupUi}.
     */
    @SuppressWarnings("NullAway") // https://crbug.com/433562519
    public static TabGroupUi createTabGroupUi(
            Activity activity,
            ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimManager scrimManager,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ModalDialogManager modalDialogManager,
            ThemeColorProvider themeColorProvider,
            UndoBarThrottle undoBarThrottle,
            MonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        if (sFactoryForTesting != null) {
            return sFactoryForTesting.createTabGroupUi(
                    activity,
                    parentView,
                    browserControlsStateProvider,
                    scrimManager,
                    omniboxFocusStateSupplier,
                    bottomSheetController,
                    dataSharingTabManager,
                    tabModelSelector,
                    tabContentManager,
                    tabCreatorManager,
                    layoutStateProviderSupplier,
                    modalDialogManager,
                    themeColorProvider,
                    undoBarThrottle,
                    tabBookmarkerSupplier,
                    shareDelegateSupplier);
        }
        return new TabGroupUiCoordinator(
                activity,
                parentView,
                browserControlsStateProvider,
                scrimManager,
                omniboxFocusStateSupplier,
                bottomSheetController,
                dataSharingTabManager,
                tabModelSelector,
                tabContentManager,
                tabCreatorManager,
                layoutStateProviderSupplier,
                modalDialogManager,
                themeColorProvider,
                undoBarThrottle,
                tabBookmarkerSupplier,
                shareDelegateSupplier);
    }

    static void setFactoryForTesting(@Nullable Factory factory) {
        sFactoryForTesting = factory;
        ResettersForTesting.register(() -> sFactoryForTesting = null);
    }
}
