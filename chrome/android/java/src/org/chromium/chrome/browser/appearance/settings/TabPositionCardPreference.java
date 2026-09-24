// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioButton;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;

/**
 * A preference that displays visual cards for selecting the tab position (Horizontal vs. Vertical).
 */
@NullMarked
public class TabPositionCardPreference extends ContainedRadioButtonGroupPreference {
    private final View.OnClickListener mHorizontalClickListener =
            v -> selectOption(/* isVertical= */ false);
    private final View.OnClickListener mVerticalClickListener =
            v -> selectOption(/* isVertical= */ true);

    /** Shared by both option cards; each reports its own state via {@link View#isSelected()}. */
    private final AccessibilityDelegateCompat mOptionAccessibilityDelegate =
            new AccessibilityDelegateCompat() {
                @Override
                public void onInitializeAccessibilityNodeInfo(
                        View host, AccessibilityNodeInfoCompat info) {
                    super.onInitializeAccessibilityNodeInfo(host, info);
                    // Announces the localized "radio button" role without needing a hardcoded
                    // string of our own.
                    info.setClassName(RadioButton.class.getName());
                    info.setCheckable(true);
                    info.setChecked(host.isSelected());
                }
            };

    private boolean mIsVerticalTabs;

    private @Nullable View mHorizontalOption;
    private @Nullable View mVerticalOption;

    public TabPositionCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.tab_position_card_preference);
    }

    /**
     * Sets the checked state of the tab position preference.
     *
     * @param isVertical Whether vertical tabs are enabled.
     */
    public void setCheckedState(boolean isVertical) {
        if (mIsVerticalTabs == isVertical) {
            return;
        }
        mIsVerticalTabs = isVertical;
        updateSelectionStates();
    }

    /** Returns whether vertical tabs are currently selected in this preference. */
    public boolean isVerticalTabsSelected() {
        return mIsVerticalTabs;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mHorizontalOption =
                NullUtil.assertNonNull(holder.findViewById(R.id.tab_position_horizontal_option));
        mVerticalOption =
                NullUtil.assertNonNull(holder.findViewById(R.id.tab_position_vertical_option));

        mHorizontalOption.setOnClickListener(mHorizontalClickListener);
        ViewCompat.setAccessibilityDelegate(mHorizontalOption, mOptionAccessibilityDelegate);

        mVerticalOption.setOnClickListener(mVerticalClickListener);
        ViewCompat.setAccessibilityDelegate(mVerticalOption, mOptionAccessibilityDelegate);

        updateSelectionStates();
    }

    private void selectOption(boolean isVertical) {
        if (mIsVerticalTabs == isVertical) {
            return;
        }
        if (callChangeListener(isVertical)) {
            setCheckedState(isVertical);
        }
    }

    private void updateSelectionStates() {
        // The preview frames duplicate their parent's state, so the selected border follows from
        // the selected option card. setSelected() already emits TYPE_VIEW_SELECTED, and the
        // delegates above report the checked state, so no manual announcement is needed here.
        if (mHorizontalOption != null) {
            mHorizontalOption.setSelected(!mIsVerticalTabs);
        }
        if (mVerticalOption != null) {
            mVerticalOption.setSelected(mIsVerticalTabs);
        }
    }

    @Nullable View getHorizontalOptionForTesting() {
        return mHorizontalOption;
    }

    @Nullable View getVerticalOptionForTesting() {
        return mVerticalOption;
    }
}
