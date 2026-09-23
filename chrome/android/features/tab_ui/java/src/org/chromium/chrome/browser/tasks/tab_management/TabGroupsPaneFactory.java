// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** Factory for creating {@link TabGroupsPane} instances for the Hub. */
@NullMarked
public class TabGroupsPaneFactory {
    private TabGroupsPaneFactory() {}

    /**
     * Creates a {@link TabGroupsPane} for the Hub.
     *
     * @param context Used to inflate UI.
     * @param tabModelSelector Used to pull tab data from.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param profileProviderSupplier The supplier for profiles.
     * @param hubManagerSupplier Supplier ultimately used to get the pane manager to switch panes.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open hidden
     *     groups.
     * @param modalDialogManagerSupplier Used to show confirmation dialogs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param dataSharingTabManager The {@link DataSharingTabManager} to start collaboration flows.
     * @return The pane implementation that displays and allows interactions with tab groups.
     */
    public static Pane createTabGroupsPane(
            Context context,
            TabModelSelector tabModelSelector,
            DoubleConsumer onToolbarAlphaChange,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            DataSharingTabManager dataSharingTabManager) {
        LazyOneshotSupplier<TabModel> tabModelSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () -> tabModelSelector.getModel(/* incognito= */ false));
        return new TabGroupsPane(
                context,
                tabModelSupplier,
                onToolbarAlphaChange,
                profileProviderSupplier,
                () -> assumeNonNull(hubManagerSupplier.get()).getPaneManager(),
                tabGroupUiActionHandlerSupplier,
                modalDialogManagerSupplier,
                edgeToEdgeSupplier,
                dataSharingTabManager);
    }
}
