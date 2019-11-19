// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.support.annotation.LayoutRes;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class TouchToFillViewHolder extends RecyclerView.ViewHolder {
    private final ViewBinder<PropertyModel, View, PropertyKey> mViewBinder;

    TouchToFillViewHolder(ViewGroup parent, @LayoutRes int layout,
            ViewBinder<PropertyModel, View, PropertyKey> viewBinder) {
        super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        mViewBinder = viewBinder;
    }

    /**
     * Called whenever an item is bound to this view holder. Please note that this method
     * might be called on the same list entry repeatedly, so make sure to always set a default
     * for unused fields.
     * @param model The {@link PropertyModel} whose data needs to be displayed.
     */
    void setupModelChangeProcessor(PropertyModel model) {
        PropertyModelChangeProcessor.create(model, itemView, mViewBinder, true);
    }
}