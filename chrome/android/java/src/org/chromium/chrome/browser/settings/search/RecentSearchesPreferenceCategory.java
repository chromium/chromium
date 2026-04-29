// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

/** {@link PreferenceCategory} used for fragment displaying recent search entries. */
@NullMarked
public class RecentSearchesPreferenceCategory extends PreferenceCategory {

    private @Nullable OnClickListener mActionListener;

    public RecentSearchesPreferenceCategory(Context context) {
        super(context);
        setLayoutResource(R.layout.settings_recent_searches_preference_category);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ButtonCompat deleteButton = (ButtonCompat) holder.findViewById(R.id.delete_button);
        if (deleteButton != null && mActionListener != null) {
            deleteButton.setText(getContext().getString(R.string.search_in_settings_recent_delete));
            deleteButton.setOnClickListener(mActionListener);
        }
    }

    public void setOnActionClickListener(OnClickListener listener) {
        mActionListener = listener;
        notifyChanged();
    }
}
