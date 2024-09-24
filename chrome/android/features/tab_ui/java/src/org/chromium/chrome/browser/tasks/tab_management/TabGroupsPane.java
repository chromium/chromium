// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationListener;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.HubLayoutConstants;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** A {@link Pane} representing the tab group UI. Contains opened and closed tab groups. */
public class TabGroupsPane implements Pane {
    private final Context mContext;
    private final LazyOneshotSupplier<TabModelFilter> mTabModelFilterSupplier;
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final FrameLayout mRootView;
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<FullButtonData> mEmptyActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();

    private TabGroupListCoordinator mTabGroupListCoordinator;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;

    /**
     * @param context Used to inflate UI.
     * @param tabModelFilterSupplier Used to pull tab data from.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param profileProviderSupplier Used to fetch the current profile.
     * @param paneManagerSupplier Used to switch and communicate with other panes.
     * @param tabGroupUiActionHandlerSupplier Used to open hidden tab groups.
     * @param modalDialogManagerSupplier Used to create confirmation dialogs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    TabGroupsPane(
            @NonNull Context context,
            @NonNull LazyOneshotSupplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull Supplier<PaneManager> paneManagerSupplier,
            @NonNull Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        mContext = context;
        mTabModelFilterSupplier = tabModelFilterSupplier;
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mProfileProviderSupplier = profileProviderSupplier;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mReferenceButtonSupplier.set(
                new ResourceButtonData(
                        R.string.accessibility_tab_groups,
                        R.string.accessibility_tab_groups,
                        R.drawable.ic_features_24dp));

        mRootView = new FrameLayout(mContext);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_GROUPS;
    }

    @NonNull
    @Override
    public ViewGroup getRootView() {
        return mRootView;
    }

    @Nullable
    @Override
    public MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
        return null;
    }

    @Override
    public boolean getMenuButtonVisible() {
        return false;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.DEFAULT;
    }

    @Override
    public void destroy() {
        if (mTabGroupListCoordinator != null) {
            mTabGroupListCoordinator.destroy();
            mTabGroupListCoordinator = null;
        }
        mRootView.removeAllViews();
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {}

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mTabGroupListCoordinator == null) {
            mTabGroupListCoordinator =
                    new TabGroupListCoordinator(
                            mContext,
                            (TabGroupModelFilter) mTabModelFilterSupplier.get(),
                            mProfileProviderSupplier.get(),
                            mPaneManagerSupplier.get(),
                            mTabGroupUiActionHandlerSupplier.get(),
                            mModalDialogManagerSupplier.get(),
                            mHairlineVisibilitySupplier::set,
                            mEdgeToEdgeSupplier);
            mRootView.addView(mTabGroupListCoordinator.getView());
        } else if (loadHint == LoadHint.COLD && mTabGroupListCoordinator != null) {
            destroy();
        }
    }

    @NonNull
    @Override
    public ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mEmptyActionButtonSupplier;
    }

    @NonNull
    @Override
    public ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonSupplier;
    }

    @NonNull
    @Override
    public ObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }

    @Nullable
    @Override
    public HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return null;
    }

    @NonNull
    @Override
    public HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @NonNull
    @Override
    public HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS, mOnToolbarAlphaChange);
    }
}
