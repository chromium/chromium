// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.TextMessagePreference;

public class TopicsHeaderPreference extends TextMessagePreference {
    public TopicsHeaderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
    }
}
