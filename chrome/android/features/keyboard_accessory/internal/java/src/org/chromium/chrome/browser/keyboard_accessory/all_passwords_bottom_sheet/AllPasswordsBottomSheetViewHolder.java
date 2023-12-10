// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display a credential {@link Credential}.
 */
class AllPasswordsBottomSheetViewHolder extends RecyclerView.ViewHolder {
    private final ViewBinder<PropertyModel, View, PropertyKey> mViewBinder;

    AllPasswordsBottomSheetViewHolder(
            ViewGroup parent,
            @LayoutRes int layout,
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
