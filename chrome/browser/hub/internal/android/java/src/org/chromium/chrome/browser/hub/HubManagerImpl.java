// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/**
 * Implementation of {@link HubManager} and {@link HubController}.
 *
 * <p>This class holds all the dependencies of {@link HubCoordinator} so that the Hub UI can be
 * created and torn down as needed when {@link HubLayout} visibility changes.
 */
public class HubManagerImpl implements HubManager, HubController {
    private final @NonNull Context mContext;
    private final @NonNull ObservableSupplierImpl<Boolean> mHubVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final @NonNull PaneManagerImpl mPaneManager;
    private final @NonNull HubContainerView mHubContainerView;
    private final @NonNull BackPressManager mBackPressManager;
    private final @NonNull ObservableSupplier<Tab> mTabSupplier;

    // This is effectively NonNull and final once the HubLayout is initialized.
    private HubLayoutController mHubLayoutController;

    private HubCoordinator mHubCoordinator;

    /** See {@link HubManagerFactory#createHubManager}. */
    public HubManagerImpl(
            @NonNull Context context,
            @NonNull PaneListBuilder paneListBuilder,
            @NonNull BackPressManager backPressManager,
            @NonNull ObservableSupplier<Tab> tabSupplier) {
        mContext = context;
        mPaneManager = new PaneManagerImpl(paneListBuilder, mHubVisibilitySupplier);
        mBackPressManager = backPressManager;
        mTabSupplier = tabSupplier;

        // TODO(crbug/1487315): Consider making this a xml file so the entire core UI is inflated.
        mHubContainerView = new HubContainerView(mContext);
    }

    @Override
    public void destroy() {
        mPaneManager.destroy();
        destroyHubCoordinator();
    }

    @Override
    public @NonNull PaneManager getPaneManager() {
        return mPaneManager;
    }

    @Override
    public @NonNull HubController getHubController() {
        return this;
    }

    @Override
    public void setHubLayoutController(@NonNull HubLayoutController hubLayoutController) {
        assert mHubLayoutController == null : "setHubLayoutController should only be called once.";
        mHubLayoutController = hubLayoutController;
    }

    @Override
    public @NonNull HubContainerView getContainerView() {
        assert mHubCoordinator != null : "Access of a HubContainerView with no descendants.";
        return mHubContainerView;
    }

    @Override
    public void onHubLayoutShow() {
        mHubVisibilitySupplier.set(true);
        ensureHubCoordinatorIsInitialized();
    }

    @Override
    public void onHubLayoutDoneHiding() {
        // TODO(crbug/1487315): Consider deferring this destruction till after a timeout.
        mHubContainerView.removeAllViews();
        destroyHubCoordinator();
        mHubVisibilitySupplier.set(false);
    }

    @Override
    public boolean onHubLayoutBackPressed() {
        if (mHubCoordinator == null) return false;

        switch (mHubCoordinator.handleBackPress()) {
            case BackPressResult.SUCCESS:
                return true;
            case BackPressResult.FAILURE:
                return false;
            default:
                assert false : "Not reached.";
                return false;
        }
    }

    private void ensureHubCoordinatorIsInitialized() {
        if (mHubCoordinator != null) return;

        assert mHubLayoutController != null
                : "HubLayoutController should be set before creating HubCoordinator.";

        mHubCoordinator =
                new HubCoordinator(
                        mHubContainerView, mPaneManager, mHubLayoutController, mTabSupplier);
        mBackPressManager.addHandler(mHubCoordinator, BackPressHandler.Type.HUB);
    }

    private void destroyHubCoordinator() {
        if (mHubCoordinator != null) {
            mBackPressManager.removeHandler(mHubCoordinator);
            mHubCoordinator.destroy();
            mHubCoordinator = null;
        }
    }

    HubCoordinator getHubCoordinatorForTesting() {
        return mHubCoordinator;
    }
}
