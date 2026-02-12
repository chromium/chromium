// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

@NullMarked
public class CustomSearchEngineListPreference extends ChromeBasePreference {

    public interface OnViewBindListener {
        void onViewBound(View view);
    }

    private @Nullable OnViewBindListener mListener;

    public CustomSearchEngineListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.custom_search_engine_list_preference);
    }

    public void setOnViewBindListener(OnViewBindListener listener) {
        mListener = listener;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        if (mListener != null) {
            mListener.onViewBound(holder.itemView);
        }
    }
}
