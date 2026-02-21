// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

@NullMarked
public class SearchEngineListPreference extends ChromeBasePreference {
    private @Nullable Adapter mAdapter;

    public SearchEngineListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.preference_list_recycler_view);
    }

    public void setAdapter(@Nullable Adapter adapter) {
        mAdapter = adapter;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        RecyclerView recyclerView = (RecyclerView) holder.itemView;
        recyclerView.setAdapter(mAdapter);
    }
}
