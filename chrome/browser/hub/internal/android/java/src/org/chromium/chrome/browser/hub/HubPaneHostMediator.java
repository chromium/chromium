// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {

    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull ObservableSupplier<Pane> mPaneSupplier;
    private final @NonNull Callback<Pane> mOnPaneChangeCallback = this::onPaneChange;
    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;

    private @Nullable ObservableSupplier<FullButtonData> mCurrentButtonDataSupplier;

    /** Creates the mediator. */
    public HubPaneHostMediator(
            @NonNull PropertyModel propertyModel, @NonNull ObservableSupplier<Pane> paneSupplier) {
        mPropertyModel = propertyModel;
        mPaneSupplier = paneSupplier;
        if (HubFieldTrial.usesFloatActionButton()) {
            mPaneSupplier.addObserver(mOnPaneChangeCallback);
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        mPaneSupplier.removeObserver(mOnPaneChangeCallback);
        if (mCurrentButtonDataSupplier != null) {
            mCurrentButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
        }
    }

    private void onPaneChange(@Nullable Pane pane) {
        if (mCurrentButtonDataSupplier != null) {
            mCurrentButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
        }

        if (pane == null) {
            mPropertyModel.set(ACTION_BUTTON_DATA, null);
        } else {
            mCurrentButtonDataSupplier = pane.getActionButtonDataSupplier();
            if (mCurrentButtonDataSupplier != null) {
                mCurrentButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
            } else {
                mPropertyModel.set(ACTION_BUTTON_DATA, null);
            }
        }
    }

    private void onActionButtonChange(FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }
}
