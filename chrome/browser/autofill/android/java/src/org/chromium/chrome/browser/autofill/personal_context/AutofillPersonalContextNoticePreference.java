// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.personal_context;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeExpandableSwitchPreference;

/**
 * Temporary subclass of {@link ChromeExpandableSwitchPreference} only here to hide the switch
 * widget for now. In the future, a toggle will be needed here, at which point this class can be
 * removed and {@link ChromeExpandableSwitchPreference} used directly.
 */
@NullMarked
public class AutofillPersonalContextNoticePreference extends ChromeExpandableSwitchPreference {
    public AutofillPersonalContextNoticePreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // TODO(crbug.com/531644749): Remove this class and use ChromeExpandableSwitchPreference
        // directly once the switch widget is needed on this preference tile.
        View switchView = holder.findViewById(android.R.id.switch_widget);
        if (switchView != null) {
            switchView.setVisibility(View.GONE);
        }
    }
}
