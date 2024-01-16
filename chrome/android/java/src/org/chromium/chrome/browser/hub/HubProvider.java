// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/**
 * Main entrypoint for providing core Hub objects to Chrome.
 *
 * <p>Part of chrome/android/ to use {@link HubManagerFactory} and to use as glue code.
 */
public class HubProvider {
    private final @NonNull LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final @NonNull PaneListBuilder mPaneListBuilder;
    private final Callback<Pane> mOnPaneFocused;

    /**
     * @param context The Android {@link Context} for the Hub.
     * @param orderController The {@link PaneOrderController} for the Hub.
     * @param backPressManager The {@link BackPressManager} for the activity.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *     activity.
     * @param snackbarManagerSupplier The supplier of the primary {@link SnackbarManager} for the
     *     activity.
     * @param tabModelSelectorSupplier The supplier of the {@link TabModelSelector}.
     * @param menuButtonCoordinatorSupplier A supplier for the root component for the app menu.
     */
    public HubProvider(
            @NonNull Context context,
            @NonNull PaneOrderController orderController,
            @NonNull BackPressManager backPressManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull Supplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<MenuButtonCoordinator> menuButtonCoordinatorSupplier) {
        mPaneListBuilder = new PaneListBuilder(orderController);
        mHubManagerSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () -> {
                            assert tabModelSelectorSupplier.hasValue();
                            ObservableSupplier<Tab> tabSupplier =
                                    tabModelSelectorSupplier.get().getCurrentTabSupplier();
                            assert menuButtonCoordinatorSupplier.hasValue();

                            SnackbarManager snackbarManager = snackbarManagerSupplier.get();
                            assert snackbarManager != null;
                            return HubManagerFactory.createHubManager(
                                    context,
                                    mPaneListBuilder,
                                    backPressManager,
                                    menuOrKeyboardActionController,
                                    snackbarManager,
                                    tabSupplier,
                                    menuButtonCoordinatorSupplier.get());
                        });

        // Taken from IncognitoToggleTabLayout.
        mOnPaneFocused =
                pane -> {
                    boolean isIncognito = pane.getPaneId() == PaneId.INCOGNITO_TAB_SWITCHER;
                    TabModelSelector selector = tabModelSelectorSupplier.get();
                    if (selector.isIncognitoSelected() == isIncognito) return;

                    selector.commitAllTabClosures();
                    selector.selectModel(isIncognito);
                    if (isIncognito) {
                        Integer tabCount = selector.getCurrentModelTabCountSupplier().get();
                        RecordHistogram.recordBooleanHistogram(
                                "Android.TabSwitcher.IncognitoClickedIsEmpty",
                                tabCount == null ? true : tabCount.intValue() == 0);
                    }
                };
        mHubManagerSupplier.onAvailable(
                hubManager -> {
                    hubManager
                            .getPaneManager()
                            .getFocusedPaneSupplier()
                            .addObserver(mOnPaneFocused);
                });
    }

    /** Destroys the {@link HubManager} it cannot be used again. */
    public void destroy() {
        HubManager hubManager = mHubManagerSupplier.get();
        if (hubManager == null) return;

        hubManager.getPaneManager().getFocusedPaneSupplier().removeObserver(mOnPaneFocused);
        hubManager.destroy();
    }

    /** Returns the lazy supplier for {@link HubManager}. */
    public @NonNull LazyOneshotSupplier<HubManager> getHubManagerSupplier() {
        return mHubManagerSupplier;
    }

    /**
     * Returns the {@link PaneListBuilder} for registering Hub {@link Pane}s. Registering a pane
     * throws an {@link IllegalStateException} once {@code #get()} is invoked on the result of
     * {@link #getHubManagerSupplier()}.
     */
    public @NonNull PaneListBuilder getPaneListBuilder() {
        return mPaneListBuilder;
    }
}
