// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;

import androidx.annotation.NonNull;

/**
 * Implementation of {@link HubManager} and {@link HubController}.
 *
 * <p>This class holds all the dependencies of {@link HubCoordinator} so that the Hub UI can be
 * created and torn down as needed when {@link HubLayout} visibility changes.
 */
public class HubManagerImpl implements HubManager, HubController {
    // Dependencies:
    private final Context mContext;

    // Final Hub specific fields:
    private final PaneManagerImpl mPaneManager;
    private final HubContainerView mHubContainerView;

    private HubCoordinator mHubCoordinator;

    /**
     * Create a {@link HubManagerImpl}.
     *
     * @param context The current {@link Context}.
     * @param paneListBuilder The {@link PaneListBuilder} consumed to build the {@link PaneManager}.
     */
    public HubManagerImpl(Context context, PaneListBuilder paneListBuilder) {
        mContext = context;
        mPaneManager = new PaneManagerImpl(paneListBuilder);

        // TODO(crbug/1487315): Consider making this a xml file so the entire core UI is inflated.
        mHubContainerView = new HubContainerView(mContext);
    }

    // HubManager implementation.

    @Override
    public @NonNull PaneManager getPaneManager() {
        return mPaneManager;
    }

    @Override
    public @NonNull HubController getHubController() {
        return this;
    }

    // HubController implementation.

    @Override
    public @NonNull HubContainerView getContainerView() {
        assert mHubCoordinator != null : "Access of a HubContainerView with no descendants.";
        return mHubContainerView;
    }

    @Override
    public void onHubLayoutShow() {
        ensureHubCoordinatorIsInitialized();
    }

    @Override
    public void onHubLayoutDoneHiding() {
        // TODO(crbug/1487315): Consider deferring this destruction till after a timeout.
        mHubContainerView.removeAllViews();
        if (mHubCoordinator != null) {
            mHubCoordinator.destroy();
            mHubCoordinator = null;
        }
    }

    private void ensureHubCoordinatorIsInitialized() {
        if (mHubCoordinator != null) return;

        mHubCoordinator =
                new HubCoordinator(mHubContainerView, mPaneManager.getFocusedPaneSupplier());
    }

    HubCoordinator getHubCoordinatorForTesting() {
        return mHubCoordinator;
    }
}
