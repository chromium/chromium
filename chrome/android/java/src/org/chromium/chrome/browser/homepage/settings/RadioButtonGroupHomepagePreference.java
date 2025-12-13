// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText.OnTextChangeListener;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A radio button group Preference used for Homepage Preference. It contains 2 options: a {@link
 * RadioButtonWithDescription} that represent Chrome NTP, and a {@link RadioButtonWithEditText} that
 * represents customized URL set by partner or user.
 */
@NullMarked
public final class RadioButtonGroupHomepagePreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener, OnTextChangeListener, ContainmentItem {
    /** A data structure which holds the displayed value and the status for this preference. */
    static class PreferenceValues {
        /** The option that is checked in {@link RadioButtonGroupHomepagePreference}. */
        private @HomepageOption int mCheckedOption;

        /** The string that displayed on the edit text box. */
        private String mCustomizedText;

        /** Whether the RadioButtonGroup is enabled. */
        private final boolean mIsEnabled;

        /** Whether the option for to {@link HomepageOption#ENTRY_CHROME_NTP} is visible. */
        private final boolean mIsNtpOptionVisible;

        /** Whether the option for to {@link HomepageOption#ENTRY_CUSTOM_URI} is visible. */
        private final boolean mIsCustomizedOptionVisible;

        /**
         * Created the data structure for {@link RadioButtonGroupHomepagePreference} to communicate
         * the preference status to outside fragment.
         *
         * @param checkedOption The option that is checked in {@link
         *     RadioButtonGroupHomepagePreference}
         * @param customizedText The string that displayed on the edit text box.
         * @param isEnabled Whether the RadioButtonGroup is enabled.
         * @param isNtpButtonVisible Whether the option for to {@link
         *     HomepageOption#ENTRY_CHROME_NTP} is visible.
         * @param isCustomizedOptionVisible Whether the option for to {@link
         *     HomepageOption#ENTRY_CUSTOM_URI} is visible.
         */
        PreferenceValues(
                @HomepageOption int checkedOption,
                String customizedText,
                boolean isEnabled,
                boolean isNtpButtonVisible,
                boolean isCustomizedOptionVisible) {
            mCheckedOption = checkedOption;
            mCustomizedText = customizedText;

            mIsEnabled = isEnabled;
            mIsNtpOptionVisible = isNtpButtonVisible;
            mIsCustomizedOptionVisible = isCustomizedOptionVisible;
        }

        /**
         * @return The option that is checked in {@link RadioButtonGroupHomepagePreference}.
         */
        @HomepageOption
        int getCheckedOption() {
            return mCheckedOption;
        }

        /**
         * @return The string that displayed on the edit text box.
         */
        String getCustomURI() {
            return mCustomizedText;
        }
    }

    /** Enums that represent the status of radio buttons inside this Preference. */
    @IntDef({HomepageOption.ENTRY_CHROME_NTP, HomepageOption.ENTRY_CUSTOM_URI})
    @Retention(RetentionPolicy.SOURCE)
    @interface HomepageOption {
        int ENTRY_CHROME_NTP = 0;
        int ENTRY_CUSTOM_URI = 1;

        int NUM_ENTRIES = 2;
    }

    /** Listener used to receive updates from this preference. */
    interface OnHomepagePreferenceChangeListener {
        void onHomepagePreferenceChange(PreferenceValues newValues);
    }

    private boolean mIsBoundToViewHolder;

    private RadioButtonWithEditText mCustomUri;
    private RadioButtonWithDescription mChromeNtp;

    private RadioButtonWithDescriptionLayout mGroup;
    private TextView mTitle;

    private View mView;
    private @Nullable PreferenceValues mPreferenceValues;
    private @Nullable ManagedPreferenceDelegate mManagedPrefDelegate;
    private @Nullable OnHomepagePreferenceChangeListener mListener;

    public RadioButtonGroupHomepagePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the ManagedPreferenceDelegate necessary to use ManagedPreferencesUtils. */
    void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ true);
    }

    /** Sets the listener for homepage preference change. */
    void setOnHomepagePreferenceChangeListener(OnHomepagePreferenceChangeListener listener) {
        mListener = listener;
    }

    /**
     * Called when the checked radio button has changed. When the selection is cleared, checkedId is
     * -1.
     *
     * @param group The group in which the checked radio button has changed
     * @param checkedId The unique identifier of the newly checked radio button
     */
    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        assert mCustomUri.isChecked() != mChromeNtp.isChecked();
        assert mPreferenceValues != null;

        @HomepageOption
        int checkedOption =
                mChromeNtp.isChecked()
                        ? HomepageOption.ENTRY_CHROME_NTP
                        : HomepageOption.ENTRY_CUSTOM_URI;

        mPreferenceValues.mCheckedOption = checkedOption;

        if (mListener != null) {
            mListener.onHomepagePreferenceChange(mPreferenceValues);
        }
    }

    @Initializer
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mView = holder.itemView;
        mChromeNtp = (RadioButtonWithDescription) holder.findViewById(R.id.radio_button_chrome_ntp);
        mCustomUri = (RadioButtonWithEditText) holder.findViewById(R.id.radio_button_uri_edit);

        assert mChromeNtp != null : "Chrome NTP button missing in the layout";
        assert mCustomUri != null : "Custom URI button missing in the layout";

        mGroup = (RadioButtonWithDescriptionLayout) holder.findViewById(R.id.radio_button_group);
        mGroup.setOnCheckedChangeListener(this);

        mTitle = (TextView) holder.findViewById(R.id.title);

        mIsBoundToViewHolder = true;
        // Set up views with data provided by the delegate.
        if (mPreferenceValues != null) {
            setupPreferenceValues(mPreferenceValues);
        }

        // Set up text listeners after initial value are set.
        mCustomUri.addTextChangeListener(this);
        // Bind view and preference.
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, mView);
    }

    /** Will be called when the text edit has a value change. */
    @Override
    public void onTextChanged(CharSequence newText) {
        assert mPreferenceValues != null;
        // If the text change is triggered by the preference fragment (ex. setting up when toggle
        // the switch), ignore this update.
        if (mPreferenceValues.mCustomizedText.equals(newText.toString())) return;

        mPreferenceValues.mCheckedOption = HomepageOption.ENTRY_CUSTOM_URI;
        mPreferenceValues.mCustomizedText = newText.toString();

        if (mListener != null) {
            mListener.onHomepagePreferenceChange(mPreferenceValues);
        }
    }

    /**
     * Update the current view with the latest data from the {@link PreferenceValues}. If the
     * preference is not bounded with view holder yet, the values will be stored and populated at
     * the end of {@link #onBindViewHolder(PreferenceViewHolder)}.
     *
     * @param value The {@link PreferenceValues} that should be presents by this preference.
     */
    void setupPreferenceValues(PreferenceValues value) {
        if (mIsBoundToViewHolder) {
            mGroup.setEnabled(value.mIsEnabled);
            mTitle.setEnabled(value.mIsEnabled);

            // Change the text first so that #onTextChanged will not mess up radio buttons that
            // should be checked.
            mCustomUri.setPrimaryText(value.mCustomizedText);

            if (value.mCheckedOption == HomepageOption.ENTRY_CHROME_NTP) {
                mChromeNtp.setChecked(true);
            } else {
                mCustomUri.setChecked(true);
            }

            mChromeNtp.setVisibility(value.mIsNtpOptionVisible ? View.VISIBLE : View.GONE);
            mCustomUri.setVisibility(value.mIsCustomizedOptionVisible ? View.VISIBLE : View.GONE);
        }

        // Lastly, store the value as current value the preference is displaying.
        // Doing it last to keep the previous value for necessary comparing in #onTextChanged.
        mPreferenceValues = value;
    }

    /** Schedules an update of the managed-state views (e.g., managed_disclaimer_text). */
    public void scheduleManagedViewUpdate() {
        if (mView != null) {
            mView.post(
                    () -> {
                        if (mManagedPrefDelegate != null && mView != null) {
                            ManagedPreferencesUtils.onBindViewToPreference(
                                    mManagedPrefDelegate, this, mView);
                        }
                    });
        }
    }

    /**
     * @return The current preference value stored in the preference.
     */
    PreferenceValues getPreferenceValue() {
        assert mPreferenceValues != null;
        return mPreferenceValues;
    }

    @VisibleForTesting
    RadioButtonWithEditText getCustomUriRadioButton() {
        return mCustomUri;
    }

    @VisibleForTesting
    RadioButtonWithDescription getChromeNtpRadioButton() {
        return mChromeNtp;
    }

    @VisibleForTesting
    TextView getTitleTextView() {
        return mTitle;
    }

    View getViewForTesting() {
        return mView;
    }
}
