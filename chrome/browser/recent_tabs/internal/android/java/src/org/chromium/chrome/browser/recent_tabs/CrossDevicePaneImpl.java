// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
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
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;

import java.util.function.DoubleConsumer;

/**
 * A {@link Pane} representing tabs from other devices. This feature is being migrated here from the
 * Recent Tabs page and used to exist under the foreign session tabs section.
 */
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

    private CrossDeviceListCoordinator mCrossDeviceListCoordinator;

    /**
     * @param context Used to inflate UI.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    CrossDevicePaneImpl(
            @NonNull Context context,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
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
