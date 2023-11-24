// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceViewHolder;

import org.chromium.ui.widget.TextViewWithClickableSpans;

/** Like a regular PreferenceCategory but with a summary text that can contain link. */
public class PreferenceCategoryWithClickableSummary extends PreferenceCategory {
    public PreferenceCategoryWithClickableSummary(
            @NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.category_with_clickable_summary_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        var summaryView = (TextViewWithClickableSpans) holder.findViewById(android.R.id.summary);
        summaryView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
