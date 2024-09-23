// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.View;
import android.widget.FrameLayout;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * An interface that allows different facilitated payment views to be shown in the same bottom
 * sheet.
 */
interface FacilitatedPaymentsSequenceView {
    /**
     * Inflates the view to be shown, and binds it to a model. Does not attach the view to the
     * {@code viewContainer}.
     *
     * @param viewContainer The {@link FrameLayout} which will hold this view when it is shown.
     */
    void setupView(FrameLayout viewContainer);

    /**
     * @return The {@link View} to be shown.
     */
    View getView();

    /**
     * @return The {@link PropertyModel} that can be used to manipulate the view returned by {@link
     *     #getView()}.
     */
    PropertyModel getModel();

    /**
     * @return The vertical scroll offset of the {@link View}.
     */
    int getVerticalScrollOffset();
}
