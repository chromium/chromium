// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/**
 * Main entrypoint for providing core Hub objects to Chrome.
 *
 * <p>Part of chrome/android/ to use {@link HubManagerFactory} and to use as glue code.
 */
public class HubProvider {
    private final @NonNull LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final @NonNull PaneListBuilder mPaneListBuilder;
    private final @NonNull Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final @NonNull Callback<Pane> mOnPaneFocused;
    private final @NonNull HubShowPaneHelper mHubShowPaneHelper;

    private @Nullable CallbackController mCallbackController = new CallbackController();
    private @Nullable HubTabSwitcherMetricsRecorder mHubTabSwitcherMetricsRecorder;

    /**
     * @param profileProviderSupplier Used to fetch dependencies.
     * @param orderController The {@link PaneOrderController} for the Hub.
     * @param backPressManager The {@link BackPressManager} for the activity.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *     activity.
     * @param snackbarManagerSupplier The supplier of the primary {@link SnackbarManager} for the
     *     activity.
     * @param tabModelSelectorSupplier The supplier of the {@link TabModelSelector}.
     * @param menuButtonCoordinatorSupplier A supplier for the root component for the app menu.
     * @param edgeToEdgeSupplier A supplier for the {@link EdgeToEdgeController}.
     * @param searchActivityClient A client for the search activity, used to launch search.
     */
    public HubProvider(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull PaneOrderController orderController,
            @NonNull BackPressManager backPressManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull Supplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<MenuButtonCoordinator> menuButtonCoordinatorSupplier,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull SearchActivityClient searchActivityClient) {
        mPaneListBuilder = new PaneListBuilder(orderController);
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mHubShowPaneHelper = new HubShowPaneHelper();
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
                                    activity,
                                    profileProviderSupplier,
                                    mPaneListBuilder,
                                    backPressManager,
                                    menuOrKeyboardActionController,
                                    snackbarManager,
                                    tabSupplier,
                                    menuButtonCoordinatorSupplier.get(),
                                    mHubShowPaneHelper,
                                    edgeToEdgeSupplier,
                                    searchActivityClient);
                        });

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
                mCallbackController.makeCancelable(this::onHubManagerAvailable));
    }

    /** Destroys the {@link HubManager} it cannot be used again. */
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mHubTabSwitcherMetricsRecorder != null) {
            mHubTabSwitcherMetricsRecorder.destroy();
            mHubTabSwitcherMetricsRecorder = null;
        }

        if (mHubManagerSupplier.hasValue()) {
            HubManager hubManager = mHubManagerSupplier.get();
            hubManager.getPaneManager().getFocusedPaneSupplier().removeObserver(mOnPaneFocused);
            hubManager.destroy();
        }
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

    /**
     * Returns the {@link HubShowPaneHelper} used to select a pane to before opening the {@link
     * HubLayout}.
     */
    public @NonNull HubShowPaneHelper getHubShowPaneHelper() {
        return mHubShowPaneHelper;
    }

    private void onHubManagerAvailable(@NonNull HubManager hubManager) {
        var focusedPaneSupplier = hubManager.getPaneManager().getFocusedPaneSupplier();
        focusedPaneSupplier.addObserver(mOnPaneFocused);
        mHubTabSwitcherMetricsRecorder =
                new HubTabSwitcherMetricsRecorder(
                        mTabModelSelectorSupplier.get(),
                        hubManager.getHubVisibilitySupplier(),
                        focusedPaneSupplier);
    }
}
