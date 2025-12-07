// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SLIDE_ANIMATE_LEFT_TO_RIGHT;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Logic for hosting a single pane at a time in the Hub. */
@NullMarked
public class HubPaneHostMediator {
    private final Callback<Pane> mOnPaneChangeCallback = this::onPaneChange;
    private final PropertyModel mPropertyModel;
    private final PaneOrderController mPaneOrderController;
    private @PaneId int mCurrentPaneId;
    private final ObservableSupplier<Pane> mPaneSupplier;

    /**
     * Should be non-null after constructor finishes, cannot be final as the Java compiler can't
     * figure this out.
     */
    private ViewGroup mSnackbarContainer;

    /**
     * Creates the mediator.
     *
     * @param propertyModel The model for the pane host.
     * @param paneSupplier The supplier for the current pane.
     * @param paneOrderController The controller for the order of panes.
     * @param defaultPaneId The default pane's Id.
     */
    public HubPaneHostMediator(
            PropertyModel propertyModel,
            ObservableSupplier<Pane> paneSupplier,
            PaneOrderController paneOrderController,
            @PaneId int defaultPaneId) {
        mPropertyModel = propertyModel;
        mPaneOrderController = paneOrderController;
        mCurrentPaneId = defaultPaneId;
        mPaneSupplier = paneSupplier;
        mPaneSupplier.addObserver(mOnPaneChangeCallback);

        // This sets mSnackbarContainer to non-null.
        propertyModel.set(SNACKBAR_CONTAINER_CALLBACK, this::consumeSnackbarContainer);
        assert mSnackbarContainer != null;
    }

    /** Cleans up observers. */
    public void destroy() {
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPaneSupplier.removeObserver(mOnPaneChangeCallback);
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mSnackbarContainer;
    }

    private void onPaneChange(@Nullable Pane pane) {
        View view = pane == null ? null : pane.getRootView();
        boolean slideLeftToRight = false; // Default/fallback direction.

        if (pane != null) {
            int newPaneId = pane.getPaneId();
            List<Integer> paneOrderList = mPaneOrderController.getPaneOrder().asList();
            int currentIndex = paneOrderList.indexOf(mCurrentPaneId);
            int newIndex = paneOrderList.indexOf(newPaneId);

            if (currentIndex != -1 && newIndex != -1) {
                // If the new pane is located to the right of the current pane in hub pane switcher,
                // slide from right to left in the hub host view.
                slideLeftToRight = newIndex < currentIndex;
            }
            mCurrentPaneId = newPaneId;
        }

        mPropertyModel.set(SLIDE_ANIMATE_LEFT_TO_RIGHT, slideLeftToRight);
        mPropertyModel.set(PANE_ROOT_VIEW, view);
    }

    private void consumeSnackbarContainer(ViewGroup snackbarContainer) {
        mSnackbarContainer = snackbarContainer;
    }
}
