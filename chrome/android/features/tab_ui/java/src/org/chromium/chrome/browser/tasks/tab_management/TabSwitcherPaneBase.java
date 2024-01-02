// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.HubLayoutConstants;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * An abstract {@link Pane} representing a tab switcher for shared logic between the normal and
 * incognito modes.
 */
public abstract class TabSwitcherPaneBase implements Pane {
    protected final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplierImpl<FullButtonData> mNewTabButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabSwitcherPaneCoordinator>
            mTabSwitcherPaneCoordinatorSupplier = new ObservableSupplierImpl<>();
    private final TransitiveObservableSupplier<TabSwitcherPaneCoordinator, Boolean>
            mHandleBackPressChangedSupplier =
                    new TransitiveObservableSupplier<>(
                            mTabSwitcherPaneCoordinatorSupplier,
                            pc -> pc.getHandleBackPressChangedSupplier());
    private final ViewGroup mRootView;
    private final TabSwitcherPaneCoordinatorFactory mFactory;

    private boolean mNativeInitialized;
    private OnClickListener mNewTabButtonClickListener;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param newTabButtonContentDescriptionRes The resource for the new tab button content
     *     description.
     */
    TabSwitcherPaneBase(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull OnClickListener newTabButtonClickListener,
            @StringRes int newTabButtonContentDescriptionRes) {
        mFactory = factory;

        mNewTabButtonDataSupplier.set(
                new DelegateButtonData(
                        new ResourceButtonData(
                                org.chromium.chrome.browser.toolbar.R.string.button_new_tab,
                                newTabButtonContentDescriptionRes,
                                org.chromium.chrome.browser.toolbar.R.drawable.new_tab_icon),
                        () -> newTabButtonClickListener.onClick(null)));

        mRootView = new FrameLayout(context);
    }

    @Override
    public void destroy() {
        destroyTabSwitcherPaneCoordinator();
    }

    @Override
    public @NonNull View getRootView() {
        return mRootView;
    }

    @Override
    public @NonNull ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mNewTabButtonDataSupplier;
    }

    @Override
    public @NonNull ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonDataSupplier;
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        // TODO(crbug/1505772): Replace with shrink animator.
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS);
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        // TODO(crbug/1505772): Replace with expand animator.
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return BackPressResult.FAILURE;
        return coordinator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    public void initWithNative() {
        if (mNativeInitialized) return;

        mNativeInitialized = true;
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator != null) {
            coordinator.initWithNative();
        }
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return null;
        return coordinator.getTabGridDialogVisibilitySupplier();
    }

    /** Returns a {@link TabSwitcherCustomViewManager} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return null;
        return coordinator.getTabSwitcherCustomViewManager();
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return 0;
        return coordinator.getTabSwitcherTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.setTabSwitcherRecyclerViewPosition(position);
    }

    protected @Nullable TabSwitcherPaneCoordinator getTabSwitcherPaneCoordinator() {
        return mTabSwitcherPaneCoordinatorSupplier.get();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    void createTabSwitcherPaneCoordinator() {
        if (mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        TabSwitcherPaneCoordinator coordinator = mFactory.create(mRootView);
        mTabSwitcherPaneCoordinatorSupplier.set(coordinator);
        if (mNativeInitialized) {
            coordinator.initWithNative();
        }
    }

    protected void destroyTabSwitcherPaneCoordinator() {
        if (!mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        mTabSwitcherPaneCoordinatorSupplier.set(null);
        mRootView.removeAllViews();
        coordinator.destroy();
    }
}
