// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;

@NullMarked
public class ClearBrowsingDataExpandablePreferenceCategory extends ExpandablePreferenceGroup {
    public ClearBrowsingDataExpandablePreferenceCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onClick() {
        setExpanded(!isExpanded());
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        assert titleView != null;
        titleView.setTextAppearance(R.style.TextAppearance_TextMediumThick_Accent1);
    }
}
