// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.view.Window;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.insets.InsetObserver;

import java.util.function.Supplier;

/**
 * A UI coordinator that manages the system status bar and bottom navigation bar for
 * ChromeTabbedActivity.
 *
 * <p>TODO(crbug.com/40618996): Create a base SystemUiCoordinator to own the
 * StatusBarColorController, and have this class extend that one.
 */
@NullMarked
public class TabbedSystemUiCoordinator {
    private final TabbedNavigationBarColorController mNavigationBarColorController;

    /**
     * Construct a new {@link TabbedSystemUiCoordinator}.
     *
     * @param window The {@link Window} associated with the containing activity.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param layoutManagerSupplier {@link LayoutManager} associated with the containing activity.
     * @param fullscreenManager The {@link FullscreenManager} used for containing activity
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} to detect when
     *     the UI is being drawn edge to edge.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for interacting with and
     *     checking the state of the bottom browser controls.
     * @param browserControlsStateProvider Supplies a {@link BrowserControlsStateProvider} for the
     *     browser controls.
     * @param snackbarManagerSupplier Supplies a {@link SnackbarManager} for snackbar management.
     * @param contextualSearchManagerSupplier Supplies a {@link ContextualSearchManager} to watch
     *     for changes to contextual search and the overlay panel.
     * @param bottomSheetController A {@link BottomSheetController} to interact with and watch for
     *     changes to the bottom sheet.
     * @param omniboxSuggestionsVisualState An optional {@link OmniboxSuggestionsVisualState} for
     *     access to the visual state of the omnibox suggestions.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent} for
     *     observing the visual state of keyboard accessories.
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param insetObserver An {@link InsetObserver} to listen for changes to the window insets.
     * @param edgeToEdgeManager Manages core edge-to-edge state and logic.
     */
    public TabbedSystemUiCoordinator(
            Window window,
            TabModelSelector tabModelSelector,
            @Nullable ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            BottomControlsStacker bottomControlsStacker,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            BottomSheetController bottomSheetController,
            @Nullable OmniboxSuggestionsVisualState omniboxSuggestionsVisualState,
            ManualFillingComponentSupplier manualFillingComponentSupplier,
            ObservableSupplier<Integer> overviewColorSupplier,
            InsetObserver insetObserver,
            EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper) {
        assert layoutManagerSupplier != null;
        mNavigationBarColorController =
                new TabbedNavigationBarColorController(
                        window.getContext(),
                        tabModelSelector,
                        layoutManagerSupplier,
                        fullscreenManager,
                        edgeToEdgeControllerSupplier,
                        bottomControlsStacker,
                        browserControlsStateProvider,
                        snackbarManagerSupplier,
                        contextualSearchManagerSupplier,
                        bottomSheetController,
                        omniboxSuggestionsVisualState,
                        manualFillingComponentSupplier,
                        overviewColorSupplier,
                        insetObserver,
                        edgeToEdgeSystemBarColorHelper);
    }

    /** Gets the {@link TabbedNavigationBarColorController}. */
    @Nullable TabbedNavigationBarColorController getNavigationBarColorController() {
        return mNavigationBarColorController;
    }

    public void destroy() {
        if (mNavigationBarColorController != null) mNavigationBarColorController.destroy();
    }
}
