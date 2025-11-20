// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** The view holding {@link BottomSheetListItemView} in a bottom sheet. */
@NullMarked
public class BottomSheetListContainerView extends LinearLayout implements ListContainerView {

    public BottomSheetListContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Adds list items views to this container view.
     *
     * @param delegate The delegate contains the content for each list item view.
     */
    @Override
    public void renderAllListItems(ListContainerViewDelegate delegate) {
        List<Integer> types = delegate.getListItems();
        for (int i = 0; i < types.size(); i++) {
            Integer type = types.get(i);
            BottomSheetListItemView listItemView = (BottomSheetListItemView) createListItemView();
            listItemView.setId(delegate.getListItemId(type));
            listItemView.setTitle(delegate.getListItemTitle(type, getContext()));
            listItemView.setSubtitle(delegate.getListItemSubtitle(type, getContext()));
            listItemView.setBackground(NtpCustomizationUtils.getBackground(types.size(), i));
            listItemView.setTrailingIcon(delegate.getTrailingIcon(type));
            listItemView.setOnClickListener(delegate.getListener(type));
            Integer descriptionResId = delegate.getTrailingIconDescriptionResId(type);
            if (descriptionResId != null) {
                listItemView.setTrailingIconContentDescriptionResId(descriptionResId);
            }

            addView(listItemView);
        }
    }

    /** Returns a {@link BottomSheetListItemView}. */
    @VisibleForTesting
    View createListItemView() {
        return LayoutInflater.from(getContext())
                .inflate(R.layout.bottom_sheet_list_item_view, this, false);
    }

    /** Clears {@link View.OnClickListener} of each list item inside this container view. */
    @Override
    public void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            BottomSheetListItemView child = (BottomSheetListItemView) getChildAt(i);
            child.setOnClickListener(null);
        }

        removeAllViews();
    }
}
