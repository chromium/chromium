// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * A radio button group Preference used for Bookmark Bar. It has 3 options: Always hide, Always
 * show, and Only show on New Tab Page.
 */
@NullMarked
public class RadioButtonGroupBookmarkBarPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener {
    private @BookmarkBarVisibilityState int mBookmarkBarState =
            BookmarkBarVisibilityState.ALWAYS_HIDE;
    private @Nullable RadioButtonWithDescriptionLayout mGroup;
    private @Nullable RadioButtonWithDescription mAlwaysShowButton;
    private @Nullable RadioButtonWithDescription mOnlyShowOnNtpButton;
    private @Nullable RadioButtonWithDescription mAlwaysHideButton;

    private @Nullable ManagedPreferenceDelegate mManagedPrefDelegate;

    public RadioButtonGroupBookmarkBarPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_bookmark_bar_preference);
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ true);
    }

    @EnsuresNonNull({"mAlwaysShowButton", "mOnlyShowOnNtpButton", "mAlwaysHideButton", "mGroup"})
    private void assertBound() {
        assert mAlwaysShowButton != null;
        assert mOnlyShowOnNtpButton != null;
        assert mAlwaysHideButton != null;
        assert mGroup != null;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mAlwaysShowButton = (RadioButtonWithDescription) holder.findViewById(R.id.always_show);
        mOnlyShowOnNtpButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.only_show_on_ntp);
        mAlwaysHideButton = (RadioButtonWithDescription) holder.findViewById(R.id.always_hide);
        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.radio_button_group);
        assert mGroup != null;
        mGroup.setOnCheckedChangeListener(this);

        setCheckedState(mBookmarkBarState);

        if (mManagedPrefDelegate != null && mManagedPrefDelegate.isPreferenceClickDisabled(this)) {
            mGroup.setEnabled(false);
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        assertBound();
        if (mAlwaysShowButton.isChecked()) {
            mBookmarkBarState = BookmarkBarVisibilityState.ALWAYS_SHOW;
        } else if (mOnlyShowOnNtpButton.isChecked()) {
            mBookmarkBarState = BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP;
        } else {
            mBookmarkBarState = BookmarkBarVisibilityState.ALWAYS_HIDE;
        }
        callChangeListener(mBookmarkBarState);
    }

    /**
     * Sets the checked state of the radio buttons.
     *
     * @param state The visibility state for the bookmark bar.
     */
    public void setCheckedState(@BookmarkBarVisibilityState int state) {
        mBookmarkBarState = state;
        if (mAlwaysShowButton != null
                && mOnlyShowOnNtpButton != null
                && mAlwaysHideButton != null) {
            mAlwaysShowButton.setChecked(state == BookmarkBarVisibilityState.ALWAYS_SHOW);
            mOnlyShowOnNtpButton.setChecked(state == BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
            mAlwaysHideButton.setChecked(state == BookmarkBarVisibilityState.ALWAYS_HIDE);
        }
    }

    public @Nullable RadioButtonWithDescription getAlwaysShowButtonForTesting() {
        return mAlwaysShowButton;
    }

    public @Nullable RadioButtonWithDescription getOnlyShowOnNtpButtonForTesting() {
        return mOnlyShowOnNtpButton;
    }

    public @Nullable RadioButtonWithDescription getAlwaysHideButtonForTesting() {
        return mAlwaysHideButton;
    }

    public @Nullable RadioButtonWithDescriptionLayout getGroupForTesting() {
        return mGroup;
    }
}
