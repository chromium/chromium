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

import org.chromium.base.metrics.RecordUserAction;
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
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
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

    private final MenuOrKeyboardActionHandler mMenuOrKeyboardActionHandler =
            new MenuOrKeyboardActionHandler() {
                @Override
                public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_select_tabs) {
                        @Nullable
                        TabSwitcherPaneCoordinator coordinator =
                                mTabSwitcherPaneCoordinatorSupplier.get();
                        if (coordinator == null) return false;

                        coordinator.showTabListEditor();
                        RecordUserAction.record("MobileMenuSelectTabs");
                        return true;
                    }
                    return false;
                }
            };
    private final ObservableSupplierImpl<TabSwitcherPaneCoordinator>
            mTabSwitcherPaneCoordinatorSupplier = new ObservableSupplierImpl<>();
    private final TransitiveObservableSupplier<TabSwitcherPaneCoordinator, Boolean>
            mHandleBackPressChangedSupplier =
                    new TransitiveObservableSupplier<>(
                            mTabSwitcherPaneCoordinatorSupplier,
                            pc -> pc.getHandleBackPressChangedSupplier());
    private final ViewGroup mRootView;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabSwitcherPaneCoordinatorFactory mFactory;

    private boolean mIsVisible;
    private boolean mNativeInitialized;
    private OnClickListener mNewTabButtonClickListener;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param menuOrKeyboardActionController Allows access to menu or keyboard actions.
     * @param newTabButtonContentDescriptionRes The resource for the new tab button content
     *     description.
     */
    TabSwitcherPaneBase(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @StringRes int newTabButtonContentDescriptionRes) {
        mFactory = factory;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;

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
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                mMenuOrKeyboardActionHandler);
        destroyTabSwitcherPaneCoordinator();
    }

    @Override
    public @NonNull View getRootView() {
        return mRootView;
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        // TODO(crbug/1502201): Figure out a more immediate signal for pane visibility. Due to
        // WARM/COLD signals being posted this can lead to multiple HOT panes for a brief period.
        // In this case multiple HOT panes might listen for the same menu event leading to a
        // collision.
        mIsVisible = loadHint == LoadHint.HOT;

        if (mIsVisible) {
            mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                    mMenuOrKeyboardActionHandler);
        } else {
            mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                    mMenuOrKeyboardActionHandler);
        }
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
        @Nullable
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
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator != null) {
            coordinator.initWithNative();
        }
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return null;
        return coordinator.getTabGridDialogVisibilitySupplier();
    }

    /** Returns a {@link TabSwitcherCustomViewManager} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return null;
        return coordinator.getTabSwitcherCustomViewManager();
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return 0;
        return coordinator.getTabSwitcherTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.setTabSwitcherRecyclerViewPosition(position);
    }

    protected @TabListMode int getTabListMode() {
        return mFactory.getTabListMode();
    }

    protected boolean isVisible() {
        return mIsVisible;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    protected @Nullable TabSwitcherPaneCoordinator getTabSwitcherPaneCoordinator() {
        return mTabSwitcherPaneCoordinatorSupplier.get();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    void createTabSwitcherPaneCoordinator() {
        if (mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        @NonNull TabSwitcherPaneCoordinator coordinator = mFactory.create(mRootView);
        mTabSwitcherPaneCoordinatorSupplier.set(coordinator);
        if (mNativeInitialized) {
            coordinator.initWithNative();
        }
    }

    protected void destroyTabSwitcherPaneCoordinator() {
        if (!mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        @NonNull TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        mTabSwitcherPaneCoordinatorSupplier.set(null);
        mRootView.removeAllViews();
        coordinator.destroy();
    }
}
