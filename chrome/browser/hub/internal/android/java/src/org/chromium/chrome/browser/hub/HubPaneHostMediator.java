// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {
    private final @NonNull Callback<Pane> mOnPangeChangeCallback = this::onPaneChange;
    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull ObservableSupplier<Pane> mPaneSupplier;

    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    /** Creates the mediator. */
    public HubPaneHostMediator(
            @NonNull PropertyModel propertyModel, @NonNull ObservableSupplier<Pane> paneSupplier) {
        mPropertyModel = propertyModel;
        mPaneSupplier = paneSupplier;
        mPaneSupplier.addObserver(mOnPangeChangeCallback);

        if (HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            paneSupplier, p -> p.getActionButtonDataSupplier());
            mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        mPaneSupplier.removeObserver(mOnPangeChangeCallback);
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
    }

    private void onPaneChange(@Nullable Pane pane) {
        View view = pane == null ? null : pane.getRootView();
        mPropertyModel.set(PANE_ROOT_VIEW, view);
    }

    private void onActionButtonChange(@Nullable FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }
}
