// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_VISIBLE;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for the action button in the Hub toolbar. */
@NullMarked
public class HubActionButtonMediator {

    private final PropertyModel mPropertyModel;

    private final Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private @Nullable ObservableSupplier<FullButtonData> mActionButtonDataSupplier;

    /** Creates the mediator. */
    public HubActionButtonMediator(PropertyModel propertyModel, PaneManager paneManager) {
        mPropertyModel = propertyModel;

        ObservableSupplier<Pane> focusedPaneSupplier = paneManager.getFocusedPaneSupplier();

        mActionButtonDataSupplier =
                focusedPaneSupplier.createTransitive(Pane::getActionButtonDataSupplier);
        mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
    }

    /** Cleans up observers. */
    public void destroy() {
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
    }

    /**
     * Updates the visibility of the action button.
     *
     * @param visible Whether the action button should be visible.
     */
    public void onActionButtonVisibilityChange(@Nullable Boolean visible) {
        mPropertyModel.set(ACTION_BUTTON_VISIBLE, Boolean.TRUE.equals(visible));
    }

    /** Handles changes to the action button data from the focused pane. */
    private void onActionButtonChange(@Nullable FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }
}
