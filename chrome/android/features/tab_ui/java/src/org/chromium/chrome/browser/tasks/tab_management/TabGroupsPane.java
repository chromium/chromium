// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneBase;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** A {@link Pane} representing the tab group UI. Contains opened and closed tab groups. */
@NullMarked
public class TabGroupsPane extends PaneBase {
    private final LazyOneshotSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<@Nullable TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final SettableMonotonicObservableSupplier<FullButtonData> mActionButtonSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mHairlineVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);
    private final DataSharingTabManager mDataSharingTabManager;

    private @Nullable TabGroupListCoordinator mTabGroupListCoordinator;
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;

    /**
     * @param context Used to inflate UI.
     * @param tabGroupModelFilterSupplier Used to pull tab data from.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param profileProviderSupplier Used to fetch the current profile.
     * @param paneManagerSupplier Used to switch and communicate with other panes.
     * @param tabGroupUiActionHandlerSupplier Used to open hidden tab groups.
     * @param modalDialogManagerSupplier Used to create confirmation dialogs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param dataSharingTabManager The {@link} DataSharingTabManager to start collaboration flows.
     */
    TabGroupsPane(
            Context context,
            LazyOneshotSupplier<TabGroupModelFilter> tabGroupModelFilterSupplier,
            DoubleConsumer onToolbarAlphaChange,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<@Nullable TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            DataSharingTabManager dataSharingTabManager) {
        super(PaneId.TAB_GROUPS, context, onToolbarAlphaChange);
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        mProfileProviderSupplier = profileProviderSupplier;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mDataSharingTabManager = dataSharingTabManager;
        TabGroupCreationUiDelegate flow =
                new TabGroupCreationUiDelegate(
                        context,
                        modalDialogManagerSupplier,
                        (Supplier<@Nullable PaneManager>) paneManagerSupplier,
                        mTabGroupModelFilterSupplier::get,
                        TabGroupCreationDialogManager::new);
        mActionButtonSupplier.set(
                new DelegateButtonData(
                        new ResourceButtonData(
                                R.string.button_new_tab_group,
                                R.string.button_new_tab_group,
                                R.drawable.new_tab_icon),
                        flow::newTabGroupFlow));
        mReferenceButtonDataSupplier.set(
                new ResourceButtonData(
                        R.string.accessibility_tab_groups,
                        R.string.accessibility_tab_groups,
                        R.drawable.ic_features_24dp));
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mTabGroupListCoordinator != null) {
            mTabGroupListCoordinator.destroy();
            mTabGroupListCoordinator = null;
        }
        mRootView.removeAllViews();
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mTabGroupListCoordinator == null) {
            mTabGroupListCoordinator =
                    new TabGroupListCoordinator(
                            mContext,
                            assertNonNull(mTabGroupModelFilterSupplier.get()),
                            assertNonNull(mProfileProviderSupplier.get()),
                            assertNonNull(mPaneManagerSupplier.get()),
                            assertNonNull(mTabGroupUiActionHandlerSupplier.get()),
                            assertNonNull(mModalDialogManagerSupplier.get()),
                            mHairlineVisibilitySupplier::set,
                            mEdgeToEdgeSupplier,
                            mDataSharingTabManager);
            mRootView.addView(mTabGroupListCoordinator.getView());
        } else if (loadHint == LoadHint.COLD && mTabGroupListCoordinator != null) {
            destroy();
        }
    }

    @Override
    public MonotonicObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mActionButtonSupplier;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }
}
