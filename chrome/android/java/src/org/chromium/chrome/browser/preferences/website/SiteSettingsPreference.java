// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.support.v4.view.ViewCompat;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;

/**
 * A custom preference for drawing Site Settings entries.
 */
public class SiteSettingsPreference extends ChromeBasePreference {
    /**
     * Constructor for inflating from XML.
     */
    public SiteSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        int padding = getContext().getResources().getDimensionPixelSize(R.dimen.pref_icon_padding);
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        ViewCompat.setPaddingRelative(
                icon, padding, icon.getPaddingTop(), 0, icon.getPaddingBottom());
    }
}
