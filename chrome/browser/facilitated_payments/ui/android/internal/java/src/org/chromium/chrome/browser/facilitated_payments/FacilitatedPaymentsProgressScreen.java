// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.ui.modelutil.PropertyModel;

// This class is used to show a progress spinner.
public class FacilitatedPaymentsProgressScreen implements FacilitatedPaymentsSequenceView {
    private View mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                LayoutInflater.from(viewContainer.getContext())
                        .inflate(
                                R.layout.facilitated_payments_progress_screen,
                                viewContainer,
                                false);
    }

    @Override
    public View getView() {
        return mView;
    }

    // The progress screen doesn't have any properties set by the feature. So its view model is
    // empty.
    @Override
    public PropertyModel getModel() {
        return new PropertyModel();
    }

    // The progress screen isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }
}
