// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for showing a user's Facilitated Payments instruments in a bottom
 * sheet.
 */
public class FacilitatedPaymentsFopSelectorScreen implements FacilitatedPaymentsSequenceView {
    private RecyclerView mView;
    private PropertyModel mModel;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (RecyclerView)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(
                                        R.layout.facilitated_payments_fop_selector,
                                        viewContainer,
                                        false);
        mView.setLayoutManager(
                new LinearLayoutManager(mView.getContext(), LinearLayoutManager.VERTICAL, false) {
                    @Override
                    public boolean isAutoMeasureEnabled() {
                        return true;
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            RecyclerView.Recycler recycler,
                            RecyclerView.State state,
                            AccessibilityNodeInfoCompat info) {}
                });

        // TODO(crbug.com/348195145): Create the FOP selector model, and bind it to the view.
        mModel = new PropertyModel();
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        return mModel;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.computeVerticalScrollOffset();
    }
}
