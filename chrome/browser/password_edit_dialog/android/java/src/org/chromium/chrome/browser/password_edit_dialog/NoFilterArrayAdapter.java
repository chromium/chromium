// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.widget.ArrayAdapter;
import android.widget.Filter;

import androidx.annotation.NonNull;

import java.util.List;

/**
 * This is subclass of ArrayAdapter, which discards any filtering.
 * Can be used in {@link android.widget.AutoCompleteTextView} to show all string items,
 * no matter if those items have any overlapping with user-typed string.
 * @param <T> type of list-based item
 */
class NoFilterArrayAdapter<T> extends ArrayAdapter<T> {
    private List<T> mItems;
    private NoFilter mFilter;

    public NoFilterArrayAdapter(@NonNull Context context, int resource, List<T> objects) {
        super(context, resource, objects);
        mItems = objects;
        mFilter = new NoFilter();
    }

    @NonNull
    @Override
    public Filter getFilter() {
        return mFilter;
    }

    class NoFilter extends Filter {
        @Override
        protected FilterResults performFiltering(CharSequence arg0) {
            FilterResults result = new FilterResults();
            result.values = mItems;
            result.count = mItems.size();
            return result;
        }

        @Override
        protected void publishResults(CharSequence arg0, FilterResults arg1) {
            notifyDataSetChanged();
        }
    }
}
