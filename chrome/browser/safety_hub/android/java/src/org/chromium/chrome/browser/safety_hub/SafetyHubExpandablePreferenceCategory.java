// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;

public class SafetyHubExpandablePreferenceCategory extends ExpandablePreferenceGroup {
    public SafetyHubExpandablePreferenceCategory(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        assert titleView != null;
        ApiCompatibilityUtils.setTextAppearance(
                titleView, R.style.TextAppearance_TextMediumThick_Accent1);
    }

    @Override
    public void onExpandedChanged(boolean expanded) {
        for (int i = 0; i < getPreferenceCount(); ++i) {
            getPreference(i).setVisible(expanded);
        }
    }

    @Override
    protected void onClick() {
        setExpanded(!isExpanded());
    }
}
