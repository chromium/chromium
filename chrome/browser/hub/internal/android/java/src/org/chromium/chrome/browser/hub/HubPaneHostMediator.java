// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.EDGE_TO_EDGE_BOTTOM_INSETS;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {
    private final @NonNull Callback<Pane> mOnPaneChangeCallback = this::onPaneChange;
    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private final @NonNull Callback<Boolean> mOnHairlineVisibilityChange =
            this::onHairlineVisibilityChange;
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull ObservableSupplier<Pane> mPaneSupplier;
    private final @NonNull TransitiveObservableSupplier<Pane, Boolean> mHairlineVisibilitySupplier;
    private final @NonNull ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final @NonNull Callback<EdgeToEdgeController> mEdgeToEdgeControllerCallback =
            this::setEdgeToEdgeController;

    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;
    private @Nullable Supplier<View> mFloatingActionButtonSupplier;
    private @Nullable EdgeToEdgeController mEdgeToEdgeController;
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /** Should be non-null after constructor finishes. */
    private ViewGroup mSnackbarContainer;

    /** Creates the mediator. */
    public HubPaneHostMediator(
            @NonNull PropertyModel propertyModel,
            @NonNull ObservableSupplier<Pane> paneSupplier,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        mPropertyModel = propertyModel;
        mPaneSupplier = paneSupplier;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mPaneSupplier.addObserver(mOnPaneChangeCallback);

        mHairlineVisibilitySupplier =
                new TransitiveObservableSupplier<>(
                        paneSupplier, p -> p.getHairlineVisibilitySupplier());
        mHairlineVisibilitySupplier.addObserver(mOnHairlineVisibilityChange);

        if (HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            paneSupplier, p -> p.getActionButtonDataSupplier());
            mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
        }

        propertyModel.set(
                FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK, this::consumeFloatingActionSupplier);
        propertyModel.set(SNACKBAR_CONTAINER_CALLBACK, this::consumeSnackbarContainer);

        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            mEdgeToEdgeSupplier.addObserver(mEdgeToEdgeControllerCallback);
            mEdgeToEdgePadAdjuster =
                    new EdgeToEdgePadAdjuster() {
                        @Override
                        public void overrideBottomInset(int inset) {
                            onEdgeToEdgeBottomInsetsChanged(inset);
                        }

                        @Override
                        public void destroy() {}
                    };
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPaneSupplier.removeObserver(mOnPaneChangeCallback);
        mHairlineVisibilitySupplier.removeObserver(mOnHairlineVisibilityChange);
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
        mEdgeToEdgeSupplier.removeObserver(mEdgeToEdgeControllerCallback);
        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.unregisterAdjuster(mEdgeToEdgePadAdjuster);
            mEdgeToEdgeController = null;
        }
    }

    /** Returns the button view for the floating action button if present. */
    public @Nullable View getFloatingActionButton() {
        return mFloatingActionButtonSupplier.get();
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mSnackbarContainer;
    }

    private void onPaneChange(@Nullable Pane pane) {
        mPropertyModel.set(COLOR_SCHEME, HubColors.getColorSchemeSafe(pane));
        View view = pane == null ? null : pane.getRootView();
        mPropertyModel.set(PANE_ROOT_VIEW, view);
    }

    private void onActionButtonChange(@Nullable FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }

    private void onHairlineVisibilityChange(@Nullable Boolean visible) {
        mPropertyModel.set(HAIRLINE_VISIBILITY, Boolean.TRUE.equals(visible));
    }

    private void onEdgeToEdgeBottomInsetsChanged(@Px int bottomInset) {
        assert EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled();

        mPropertyModel.set(EDGE_TO_EDGE_BOTTOM_INSETS, bottomInset);
    }

    void setEdgeToEdgeController(EdgeToEdgeController edgeToEdgeController) {
        assert EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled();

        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.unregisterAdjuster(mEdgeToEdgePadAdjuster);
        }
        mEdgeToEdgeController = edgeToEdgeController;
        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.registerAdjuster(mEdgeToEdgePadAdjuster);
        }
    }

    private void consumeFloatingActionSupplier(Supplier<View> floatingActionButtonSupplier) {
        mFloatingActionButtonSupplier = floatingActionButtonSupplier;
    }

    private void consumeSnackbarContainer(ViewGroup snackbarContainer) {
        mSnackbarContainer = snackbarContainer;
    }
}
