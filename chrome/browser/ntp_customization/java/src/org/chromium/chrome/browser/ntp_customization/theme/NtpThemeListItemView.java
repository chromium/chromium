// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;

/** The list item view within the "New tab page appearance" bottom sheet. */
@NullMarked
public class NtpThemeListItemView extends LinearLayout {

    public NtpThemeListItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    void destroy() {
        setOnClickListener(null);
    }

    /**
     * Set the visibility of section's trailing icon.
     *
     * @param visible Whether icon is visible.
     */
    void setTrailingIconVisibility(boolean visible) {
        ImageView ntpThemeListItemTrailingIcon = findViewById(R.id.trailing_icon);
        if (visible) {
            ntpThemeListItemTrailingIcon.setVisibility(View.VISIBLE);
        } else {
            ntpThemeListItemTrailingIcon.setVisibility(View.INVISIBLE);
        }
    }
}
