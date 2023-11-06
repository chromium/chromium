// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.supplier.ObservableSupplier;

/**
 * Root coordinator of the Hub.
 *
 * <p>TODO(crbug/1487315): This is a stub implementation make it display the Hub UI.
 */
public class HubCoordinator {
    private final FrameLayout mContainerView;
    private final View mMainHubParent;
    private final HubToolbarCoordinator mHubToolbarCoordinator;
    private final HubPaneHostCoordinator mHubPaneHostCoordinator;

    /**
     * Creates the {@link HubCoordinator}.
     *
     * @param containerView The view to attach the Hub to.
     * @param paneSupplier Used to observe the current pane.
     */
    public HubCoordinator(FrameLayout containerView, ObservableSupplier<Pane> paneSupplier) {
        Context context = containerView.getContext();
        mContainerView = containerView;
        mMainHubParent = LayoutInflater.from(context).inflate(R.layout.hub_layout, null);
        mContainerView.addView(mMainHubParent);

        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        mHubToolbarCoordinator = new HubToolbarCoordinator(hubToolbarView);

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        mHubPaneHostCoordinator = new HubPaneHostCoordinator(hubPaneHostView, paneSupplier);
    }

    /** Removes the hub from the layout tree and cleans up resources. */
    public void destroy() {
        mContainerView.removeView(mMainHubParent);
        mHubToolbarCoordinator.destroy();
        mHubPaneHostCoordinator.destroy();
    }
}
