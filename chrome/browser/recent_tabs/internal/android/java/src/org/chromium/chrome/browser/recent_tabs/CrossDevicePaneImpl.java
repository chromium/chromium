// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationListener;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;

import java.util.function.DoubleConsumer;

/**
 * A {@link Pane} representing tabs from other devices. This feature is being migrated here from the
 * Recent Tabs page and used to exist under the foreign session tabs section.
 */
@NullMarked
public class CrossDevicePaneImpl implements CrossDevicePane {
    private final Context mContext;
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final FrameLayout mRootView;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeController;
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<FullButtonData> mEmptyActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();

    private @Nullable CrossDeviceListCoordinator mCrossDeviceListCoordinator;

    /**
     * @param context Used to inflate UI.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    CrossDevicePaneImpl(
            Context context,
            DoubleConsumer onToolbarAlphaChange,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        mContext = context;
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mEdgeToEdgeController = edgeToEdgeSupplier;
        mReferenceButtonSupplier.set(
                new ResourceButtonData(
                        R.string.accessibility_cross_device_tabs,
                        R.string.accessibility_cross_device_tabs,
                        R.drawable.devices_black_24dp));

        mRootView = new FrameLayout(mContext);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.CROSS_DEVICE;
    }

    @Override
    public ViewGroup getRootView() {
        return mRootView;
    }

    @Override
    public @Nullable MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
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
        if (mCrossDeviceListCoordinator != null) {
            mCrossDeviceListCoordinator.destroy();
            mCrossDeviceListCoordinator = null;
        }
        mRootView.removeAllViews();
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {}

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT) {
            if (mCrossDeviceListCoordinator == null) {
                mCrossDeviceListCoordinator =
                        new CrossDeviceListCoordinator(mContext, mEdgeToEdgeController);
                mRootView.addView(mCrossDeviceListCoordinator.getView());
            } else {
                mCrossDeviceListCoordinator.buildCrossDeviceData();
            }
        } else if (loadHint == LoadHint.WARM && mCrossDeviceListCoordinator != null) {
            mCrossDeviceListCoordinator.clearCrossDeviceData();
        } else if (loadHint == LoadHint.COLD && mCrossDeviceListCoordinator != null) {
            destroy();
        }
    }

    @Override
    public ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mEmptyActionButtonSupplier;
    }

    @Override
    public ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonSupplier;
    }

    @Override
    public ObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }

    @Override
    public @Nullable HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return null;
    }

    @Override
    public HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @Override
    public HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @Override
    public ObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }
}
