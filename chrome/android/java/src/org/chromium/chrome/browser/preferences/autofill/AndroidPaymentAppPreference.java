// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;

/** Preference with fixed icon size for Android payment apps. */
public class AndroidPaymentAppPreference extends Preference {
    public AndroidPaymentAppPreference(Context context) {
        super(context, null);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // TODO(crbug.com/971791): Simplify this or replace it with a custom preference layout.
        int iconSize =
                getContext().getResources().getDimensionPixelSize(R.dimen.payments_favicon_size);
        View iconView = holder.findViewById(android.R.id.icon);
        ViewGroup.LayoutParams layoutParams = iconView.getLayoutParams();
        layoutParams.width = iconSize;
        layoutParams.height = iconSize;
        iconView.setLayoutParams(layoutParams);
    }
}