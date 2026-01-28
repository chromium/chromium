// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

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
     * Updates the trailing icon's visibility, image resource, and tint color.
     *
     * @param visible True to indicate a selected state, false otherwise.
     * @param sectionType The type of the NTP background image section.
     */
    void updateTrailingIcon(boolean visible, @NtpBackgroundImageType int sectionType) {
        ImageView ntpThemeListItemTrailingIcon = findViewById(R.id.trailing_icon);

        if (sectionType == NtpBackgroundImageType.DEFAULT) {
            ntpThemeListItemTrailingIcon.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
            return;
        }

        int iconResId;
        int colorId;
        if (visible) {
            iconResId = R.drawable.ic_check_googblue_24dp;
            colorId = SemanticColorUtils.getColorPrimary(getContext());
        } else {
            iconResId = R.drawable.forward_arrow_icon;
            colorId = SemanticColorUtils.getDefaultTextColorSecondary(getContext());
        }
        ntpThemeListItemTrailingIcon.setImageResource(iconResId);
        ImageViewCompat.setImageTintList(
                ntpThemeListItemTrailingIcon, ColorStateList.valueOf(colorId));
    }
}
