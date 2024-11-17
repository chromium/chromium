// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/** Factory for creating {@link HubManager}. */
public class HubManagerFactory {
    /**
     * Creates a new instance of {@link HubManagerImpl}.
     *
     * @param profileProviderSupplier Used to fetch dependencies.
     * @param paneListBuilder The {@link PaneListBuilder} which is consumed to build a {@link
     *     PaneManager}.
     * @param backPressManager The {@link BackPressManager} for the activity.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *     activity.
     * @param snackbarManager The primary {@link SnackbarManager} for the activity.
     * @param tabSupplier The supplier of the current tab in the current tab model.
     * @param menuButtonCoordinator Root component for the app menu.
     * @param edgeToEdgeSupplier A supplier to the {@link EdgeToEdgeController}.
     * @param searchActivityClient A client for the search activity, used to launch search.
     * @return an instance of {@link HubManagerImpl}.
     */
    public static HubManager createHubManager(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull PaneListBuilder paneListBuilder,
            @NonNull BackPressManager backPressManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull MenuButtonCoordinator menuButtonCoordinator,
            @NonNull HubShowPaneHelper hubShowPaneHelper,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull SearchActivityClient searchActivityClient) {
        return new HubManagerImpl(
                activity,
                profileProviderSupplier,
                paneListBuilder,
                backPressManager,
                menuOrKeyboardActionController,
                snackbarManager,
                tabSupplier,
                menuButtonCoordinator,
                hubShowPaneHelper,
                edgeToEdgeSupplier,
                searchActivityClient);
    }
}
