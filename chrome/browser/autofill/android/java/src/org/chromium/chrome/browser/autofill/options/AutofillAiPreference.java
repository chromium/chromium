// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/**
 * A regular {@link ChromeSwitchPreference} that displays information items below it. It's not
 * possible to directly combine the {@link ChromeSwitchPreference} with the text info items inside
 * the {@code LinearLayout}. This preference defines a custom layout what reuses the {@link
 * ChromeSwitchPreference} layout and adds the information items below it.
 */
@NullMarked
public class AutofillAiPreference extends ChromeSwitchPreference {

    /** Constructor for inflating from XML. */
    public AutofillAiPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.autofill_ai_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        setInfoItemDetails(
                holder.findViewById(R.id.autofill_ai_when_on),
                R.string.settings_autofill_ai_when_on,
                R.string.settings_autofill_ai_when_on_can_fill_difficult_fields,
                R.drawable.text_analysis_24dp);
        setInfoItemDetails(
                holder.findViewById(R.id.autofill_ai_things_to_consider),
                R.string.settings_autofill_ai_things_to_consider,
                R.string.settings_autofill_ai_to_consider_data_usage,
                R.drawable.google_24dp);
    }

    private void setInfoItemDetails(
            View infoItem,
            @StringRes int titleId,
            @StringRes int summaryId,
            @DrawableRes int iconId) {
        TextView titleView = (TextView) infoItem.findViewById(R.id.info_item_title);
        TextView summaryView = (TextView) infoItem.findViewById(R.id.info_item_summary);

        Context context = getContext();

        titleView.setText(context.getString(titleId));
        summaryView.setText(context.getString(summaryId));

        Drawable icon = AppCompatResources.getDrawable(context, iconId);
        if (icon != null) {
            @Px
            int iconSize =
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.autofill_ai_preference_info_item_icon_size);
            icon.setBounds(0, 0, iconSize, iconSize);
            summaryView.setCompoundDrawablesRelative(icon, null, null, null);
            summaryView.setCompoundDrawablePadding(
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.autofill_ai_preference_info_item_icon_padding));
        }
    }
}
