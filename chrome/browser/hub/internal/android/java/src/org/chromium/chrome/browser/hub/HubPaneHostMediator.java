// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {

    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;

    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    /** Creates the mediator. */
    public HubPaneHostMediator(
            @NonNull PropertyModel propertyModel, @NonNull ObservableSupplier<Pane> paneSupplier) {
        mPropertyModel = propertyModel;
        if (HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            paneSupplier, p -> p.getActionButtonDataSupplier());
            mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
    }

    private void onActionButtonChange(FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }
}
