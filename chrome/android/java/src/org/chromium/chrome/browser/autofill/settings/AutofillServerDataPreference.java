// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

/**
 * A {@link ChromeBasePreference} for server-stored payment methods that displays the Google Pay
 * logo.
 */
@NullMarked
public class AutofillServerDataPreference extends ChromeBasePreference {
    public AutofillServerDataPreference(Context context) {
        super(context);
        setWidgetLayoutResource(R.layout.autofill_server_data_label);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ImageView serverDataLabel = (ImageView) holder.findViewById(R.id.server_data_label);
        if (serverDataLabel != null) {
            serverDataLabel.setImageResource(AutofillUiUtils.getGPayLogoDrawableId());
        }
    }
}
