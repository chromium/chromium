// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.about_settings;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.Clipboard;

/** Preference that copies its summary to the clipboard upon a long press. */
@NullMarked
public class LongClickCopySummaryPreference extends Preference {
    /** Constructor for inflating from XML. */
    public LongClickCopySummaryPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        holder.itemView.setOnLongClickListener(
                v -> {
                    CharSequence title = assertNonNull(getTitle());
                    CharSequence summary = assertNonNull(getSummary());
                    Clipboard.getInstance().setText(title.toString(), summary.toString(), true);
                    return true;
                });
    }
}
