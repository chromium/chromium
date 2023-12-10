// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;

import java.util.ArrayList;
import java.util.List;

/**
 * Dropdown field adapter with proper padding for the selected item view.
 *
 * @param <T> The type of element to be inserted into the adapter.
 */
class DropdownFieldAdapter<T> extends ArrayAdapter<T> {
    /**
     * Creates an array adapter.
     *
     * @param context            The current context.
     * @param resource           The resource ID for a layout file containing a layout to use when
     *                           instantiating views.
     * @param textViewResourceId The id of the TextView within the layout resource to be populated.
     * @param objects            The objects to represent in the ListView.
     */
    public DropdownFieldAdapter(
            Context context, int resource, int textViewResourceId, List<T> objects) {
        super(context, resource, textViewResourceId, new ArrayList<T>(objects));
    }

    /**
     * Creates an array adapter.
     *
     * @param context            The current context.
     * @param resource           The resource ID for a layout file containing a layout to use when
     *                           instantiating views.
     * @param objects            The objects to represent in the ListView.
     */
    public DropdownFieldAdapter(Context context, int resource, List<T> objects) {
        super(context, resource, new ArrayList<T>(objects));
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = super.getView(position, convertView, parent);

        // Add the left and right padding of the parent's background to the selected item view to
        // avoid overlaping the downward triangle.
        Rect rect = new Rect();
        parent.getBackground().getPadding(rect);
        view.setPadding(
                view.getPaddingLeft() + rect.left,
                view.getPaddingTop(),
                view.getPaddingRight() + rect.right,
                view.getPaddingBottom());
        return view;
    }
}
