// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_VIEW_PROVIDER;

import android.view.ViewGroup;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubPaneHostView.PaneViewProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that holds a single pane at a time in the Hub. */
@NullMarked
public class HubPaneHostCoordinator {
    private final HubPaneHostMediator mMediator;
    private final PropertyModel mModel;
    private final HubPaneHostView mHubPaneHostView;

    /**
     * Eagerly creates the component, but will not be rooted in the view tree yet.
     *
     * @param hubPaneHostView The root view of this component. Inserted into hierarchy for us.
     * @param paneSupplier A way to observe and get the current {@link Pane}.
     * @param hubColorMixer Mixes the Hub Overview Color.
     * @param defaultPaneId The default pane's Id.
     * @param paneViewProvider Supplier of adjacent pane views for swipe gesture.
     */
    public HubPaneHostCoordinator(
            HubPaneHostView hubPaneHostView,
            MonotonicObservableSupplier<Pane> paneSupplier,
            HubColorMixer hubColorMixer,
            @PaneId int defaultPaneId,
            @Nullable PaneViewProvider paneViewProvider) {
        mHubPaneHostView = hubPaneHostView;
        mModel =
                new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS)
                        .with(COLOR_MIXER, hubColorMixer)
                        .with(PANE_VIEW_PROVIDER, paneViewProvider)
                        .build();
        PropertyModelChangeProcessor.create(mModel, hubPaneHostView, HubPaneHostViewBinder::bind);
        mMediator =
                new HubPaneHostMediator(
                        mModel, paneSupplier, new DefaultPaneOrderController(), defaultPaneId);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mHubPaneHostView.destroy();
        mModel.set(COLOR_MIXER, null);
        mMediator.destroy();
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mMediator.getSnackbarContainer();
    }
}
