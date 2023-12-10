// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;

/** Preference with fixed icon size for Android payment apps. */
public class AndroidPaymentAppPreference extends Preference {
    public AndroidPaymentAppPreference(Context context) {
        super(context, null);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        int iconSize =
                getContext().getResources().getDimensionPixelSize(R.dimen.payments_favicon_size);
        View iconView = holder.findViewById(android.R.id.icon);
        ViewGroup.LayoutParams layoutParams = iconView.getLayoutParams();
        layoutParams.width = iconSize;
        layoutParams.height = iconSize;
        iconView.setLayoutParams(layoutParams);
    }
}
