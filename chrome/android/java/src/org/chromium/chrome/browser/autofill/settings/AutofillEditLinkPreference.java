// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;

/**
 * A {@link Preference} that provides a clickable edit link as a widget.
 *
 * {@link OnPreferenceClickListener} is called when the link is clicked.
 */
public class AutofillEditLinkPreference extends Preference {
    /** Constructor for inflating from XML. */
    public AutofillEditLinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
        setWidgetLayoutResource(R.layout.autofill_server_data_edit_link);
        setTitle(R.string.autofill_from_google_account_long);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        View button = holder.findViewById(R.id.preference_click_target);
        button.setClickable(true);
        button.setOnClickListener(
                v -> {
                    if (getOnPreferenceClickListener() != null) {
                        getOnPreferenceClickListener()
                                .onPreferenceClick(AutofillEditLinkPreference.this);
                    }
                });
    }
}
