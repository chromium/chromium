// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroupAdapter;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;

import java.util.Map;

/**
 * Controls the highlight of the selected main menu item. We should consider migrating this into
 * Containment implementation.
 */
@NullMarked
class SelectionDecoration extends RecyclerView.ItemDecoration {
    // For historical reason, there are main menu prefs that is navigating to the same
    // fragment. To handle the case, we unify the entries.
    private static final Map<String, String> DEDUP_MAP = Map.of("sign_in", "manage_sync");

    /** Margin between items. This is added both top and bottom. */
    private final int mVerticalMarginPx;

    /**
     * Margin of the left side of the item.
     *
     * <p>This is short term workaround until crbug.com/454247949 is addressed
     */
    private final int mLeftMarginPx;

    /** Card background of the selected item. */
    private final Drawable mSelectedBackground;

    /**
     * Key of the selected preference defined in main_preference.xml. Maybe null if no entry in the
     * xml is selected, or mPreference is set.
     */
    private @Nullable String mKey;

    /**
     * Preference instance of the selected item on the main menu. May be null if the detailed page
     * is updated by something other than selecting a main menu item.
     */
    private @Nullable Preference mPreference;

    /** A flag to re-draw background decoration. */
    private boolean mIsDirty = true;

    SelectionDecoration(
            int verticalMarginPx, int leftMarginPx, float radiusPx, int selectedBackgroundColor) {
        mVerticalMarginPx = verticalMarginPx;
        mLeftMarginPx = leftMarginPx;
        mSelectedBackground =
                ContainmentViewStyler.createRoundedDrawable(
                        radiusPx, radiusPx, selectedBackgroundColor);
    }

    private static @Nullable String dedup(@Nullable String key) {
        if (key != null) {
            String dedupped = DEDUP_MAP.get(key);
            if (dedupped != null) {
                return dedupped;
            }
        }
        return key;
    }

    /**
     * Sets the key of the preference entry in main_preferences.xml to be highlighted. Setting null
     * unsets the highlight.
     */
    void setKey(@Nullable String key) {
        key = dedup(key);
        if (mPreference != null) {
            String preferenceKey = dedup(mPreference.getKey());
            if (TextUtils.equals(preferenceKey, key)) {
                return;
            }
        }
        mPreference = null;
        mKey = key;
        mIsDirty = true;
    }

    void setSelectedPreference(Preference preference) {
        if (mPreference != preference) {
            mPreference = preference;
            mKey = null;
            mIsDirty = true;
        }
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);
        mIsDirty = true;

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        layoutParams.setMargins(
                mLeftMarginPx, mVerticalMarginPx, layoutParams.rightMargin, mVerticalMarginPx);
        view.setLayoutParams(layoutParams);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (!mIsDirty) {
            return;
        }
        mIsDirty = false;
        setChildViewColor(parent);
        super.onDraw(c, parent, state);
    }

    /**
     * Updates the background of child views. If there's no focused preference, highlight will be
     * just disabled.
     */
    private void setChildViewColor(RecyclerView parent) {
        var preferenceGroupAdapter = (PreferenceGroupAdapter) parent.getAdapter();
        assert preferenceGroupAdapter != null;

        boolean highlightFound = false;
        for (int i = 0; i < parent.getChildCount(); ++i) {
            View view = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(view);
            Preference preference = preferenceGroupAdapter.getItem(position);

            boolean selected = false;
            if (!highlightFound) {
                if (mPreference != null) {
                    selected = (mPreference == preference);
                } else if (mKey != null) {
                    if (preference != null && TextUtils.equals(mKey, dedup(preference.getKey()))) {
                        selected = true;
                    }
                }
            }

            if (mKey != null && preference instanceof PreferenceCategory) {
                TextView headerTitleView = findTextView(view);
                if (headerTitleView != null) {
                    headerTitleView.setTextAppearance(
                            R.style.TextAppearance_PreferenceCategoryStandard);
                }
            }
            if (selected) {
                highlightFound = true;
                view.setBackground(mSelectedBackground);
                if (view.findViewById(android.R.id.title) instanceof TextView textView) {
                    textView.setTextAppearance(
                            R.style.TextAppearance_SettingsSelectedMainMenuItemTitle);
                }
            } else {
                view.setBackground(null);
                if (view.findViewById(android.R.id.title) instanceof TextView textView) {
                    textView.setTextAppearance(R.style.TextAppearance_SettingsMainMenuItemTitle);
                }
            }
        }
    }

    private static @Nullable TextView findTextView(View view) {
        if (view instanceof TextView textView) {
            return textView;
        }
        if (view instanceof ViewGroup viewGroup) {
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                TextView found = findTextView(viewGroup.getChildAt(i));
                if (found != null) {
                    return found;
                }
            }
        }
        return null;
    }
}
