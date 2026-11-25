// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import android.content.Context;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.TextViewCompat;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ttc.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

/**
 * A settings row showing a TTC keyboard shortcut as keycaps, or that it isn't set, with a button to
 * clear it.
 */
@NullMarked
public class TtcShortcutPreference extends ChromeBasePreference {
    private String mShortcut = "";
    private @Nullable Runnable mOnClearListener;

    /** Constructor for inflating from XML. */
    public TtcShortcutPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.ttc_shortcut_preference_widget);
        // Clicking the row does nothing yet; only its clear button does something.
        setSelectable(false);
    }

    /** Sets the shortcut to show, as a shortcut pref value, or "" if it isn't set. */
    public void setShortcut(String shortcut) {
        if (mShortcut.equals(shortcut)) return;
        mShortcut = shortcut;
        notifyChanged();
    }

    /** Returns the shortcut shown, as a shortcut pref value, or "" if it isn't set. */
    public String getShortcut() {
        return mShortcut;
    }

    /** Sets what clicking the clear button does. */
    public void setOnClearListener(@Nullable Runnable onClearListener) {
        mOnClearListener = onClearListener;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        // A value that names no shortcut, e.g. one saved on another platform, isn't set.
        LinearLayout keycaps = (LinearLayout) holder.findViewById(R.id.ttc_shortcut_keycaps);
        boolean isSet = bindKeycaps(keycaps, mShortcut);
        keycaps.setVisibility(isSet ? View.VISIBLE : View.GONE);
        holder.findViewById(R.id.ttc_shortcut_not_set)
                .setVisibility(isSet ? View.GONE : View.VISIBLE);
        View clear = holder.findViewById(R.id.ttc_shortcut_clear);
        clear.setVisibility(isSet ? View.VISIBLE : View.GONE);
        clear.setContentDescription(
                getContext().getString(R.string.ttc_settings_shortcut_clear, getTitle()));
        clear.setOnClickListener(v -> clear());
    }

    @VisibleForTesting
    void clear() {
        if (mOnClearListener != null) mOnClearListener.run();
    }

    /**
     * Shows the keys of {@code shortcut}, a shortcut pref value, as keycaps in {@code container}.
     * Returns whether it shows any, i.e. whether {@code shortcut} names a shortcut.
     */
    static boolean bindKeycaps(LinearLayout container, String shortcut) {
        container.removeAllViews();
        if (shortcut.isEmpty()) return false;
        Context context = container.getContext();
        int horizontalPadding =
                context.getResources().getDimensionPixelSize(R.dimen.ttc_keycap_padding_horizontal);
        int verticalPadding =
                context.getResources().getDimensionPixelSize(R.dimen.ttc_keycap_padding_vertical);
        int spacing = context.getResources().getDimensionPixelSize(R.dimen.ttc_keycap_spacing);
        String[] keys = TtcShortcutUtils.getKeyLabels(shortcut);
        for (int i = 0; i < keys.length; i++) {
            TextView keycap = new TextView(context);
            TextViewCompat.setTextAppearance(keycap, R.style.TextAppearance_TextSmall_Secondary);
            keycap.setTypeface(keycap.getTypeface(), Typeface.BOLD);
            keycap.setBackgroundResource(R.drawable.ttc_keycap_background);
            keycap.setPadding(
                    horizontalPadding, verticalPadding, horizontalPadding, verticalPadding);
            keycap.setText(keys[i]);
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT);
            // Space only between keycaps, so that they stay centered in a centered container.
            if (i > 0) params.setMarginStart(spacing);
            container.addView(keycap, params);
        }
        return keys.length > 0;
    }
}
