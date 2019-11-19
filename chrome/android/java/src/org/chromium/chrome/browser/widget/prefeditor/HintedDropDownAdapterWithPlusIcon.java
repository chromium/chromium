// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.prefeditor;

import android.content.Context;
import android.content.res.Resources;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.ui.UiUtils;

import java.util.List;

/**
 * Subclass of HintedDropDownAdapter used to display a hinted dropdown . The last
 * displayed element on the list will have a "+" icon on its left and a blue tint to indicate that
 * the option is for adding an element.
 *
 * @param <T> The type of element to be inserted into the adapter.
 *
 *
 * collapsed view:       --------          Expanded view:   ------------
 *                                                         . hint       .
 *                                                         ..............
 * (no item selected)   | hint   |                         | option 1   |
 *                       --------                          |------------|
 *                                                         | option 2   |
 * collapsed view:       ----------                        |------------|
 * (with selected item) | option X |                       | + option N | -> stylized and "+" icon
 *                       ----------                        .------------.
 */
public class HintedDropDownAdapterWithPlusIcon<T> extends HintedDropDownAdapter<T> {
    /**
     * Creates an array adapter for which the first element is a hint and where the
     * last displayed element has a "+" icon on its left and has a blue tint.
     *
     * @param context            The current context.
     * @param resource           The resource ID for a layout file containing a layout to use when
     *                           instantiating views.
     * @param mTextViewResourceId The id of the TextView within the layout resource to be populated.
     * @param objects            The objects to represent in the ListView, the last
     *                           item will have a "+" icon on its left and will have a blue tint.
     * @param hint               The element to be used as a hint when no element is selected.
     */
    public HintedDropDownAdapterWithPlusIcon(
            Context context, int resource, int textViewResourceId, List<T> objects, T hint) {
        super(context, resource, textViewResourceId, objects, hint);
    }

    @Override
    public View getDropDownView(int position, View convertView, ViewGroup parent) {
        convertView = super.getDropDownView(position, convertView, parent);

        // The plus icon is for the last item on the list.
        if (position == getCount() - 1) {
            // Add a "+" icon and a blue tint to the last element.
            if (mTextView == null) {
                mTextView = (TextView) convertView.findViewById(mTextViewResourceId);
            }

            // Create the "+" icon, put it left of the text and add appropriate padding.
            mTextView.setCompoundDrawablesWithIntrinsicBounds(
                    TintedDrawable.constructTintedDrawable(
                            getContext(), R.drawable.plus, R.color.default_icon_color_blue),
                    null, null, null);
            Resources resources = getContext().getResources();
            mTextView.setCompoundDrawablePadding(
                    resources.getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing));

            // Set the correct appearance, face and style for the text.
            ApiCompatibilityUtils.setTextAppearance(
                    mTextView, R.style.TextAppearance_EditorDialogSectionAddButton);
            mTextView.setTypeface(UiUtils.createRobotoMediumTypeface());

            // Padding at the bottom of the dropdown.
            ViewCompat.setPaddingRelative(convertView, ViewCompat.getPaddingStart(convertView),
                    convertView.getPaddingTop(), ViewCompat.getPaddingEnd(convertView),
                    getContext().getResources().getDimensionPixelSize(
                            R.dimen.editor_dialog_section_small_spacing));
        }

        return convertView;
    }
}
