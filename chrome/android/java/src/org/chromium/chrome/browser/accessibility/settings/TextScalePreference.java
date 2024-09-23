// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.content.Context;
import android.os.Build;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.widget.SeekBar;
import android.widget.TextView;
import org.chromium.chrome.R;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import java.text.NumberFormat;

/**
 * Preference that allows the user to change the scaling factor that's applied to web page text.
 * This also shows a preview of how large a typical web page's text will appear.
 */
public class TextScalePreference extends Preference implements SeekBar.OnSeekBarChangeListener {
    private static final float MIN = 0.5f;
    private static final float MAX = 2.0f;
    private static final float STEP = 0.05f;

    /**
     * Online body text tends to be around 13-16px. We ask the user to adjust the text scale until
     * 12px text is legible, that way all body text will be legible (and since font boosting
     * approximately preserves relative font size differences, other text will be bigger/smaller as
     * appropriate).
     */
    private static final float SMALLEST_STANDARD_FONT_SIZE_PX = 12.0f;

    private float mUserFontScaleFactor = MIN;
    private float mFontScaleFactor;

    private TextView mAmount;
    private TextView mPreview;
    private SeekBar mSeekBar;

    private NumberFormat mFormat = NumberFormat.getPercentInstance();

    /** Constructor for inflating from XML. */
    public TextScalePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.custom_preference);
        setWidgetLayoutResource(R.layout.preference_text_scale);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mSeekBar = (SeekBar) holder.findViewById(R.id.seekbar);
        mSeekBar.setOnSeekBarChangeListener(this);
        mSeekBar.setMax(userFontScaleFactorToProgress(MAX));
        mSeekBar.setProgress(userFontScaleFactorToProgress(mUserFontScaleFactor));

        mAmount = (TextView) holder.findViewById(R.id.seekbar_amount);
        mPreview = (TextView) holder.findViewById(R.id.preview);

        updateViews();
    }

    void updateFontScaleFactors(
            float fontScaleFactor, float userFontScaleFactor, boolean updateViews) {
        mFontScaleFactor = fontScaleFactor;
        mUserFontScaleFactor = userFontScaleFactor;

        if (updateViews) updateViews();
    }

    private void updateViews() {
        mAmount.setText(mFormat.format(mUserFontScaleFactor));

        // On Android R+, use stateDescription so the only percentage announced to the user is
        // the scaling percent. For previous versions the SeekBar percentage is always announced.
        String userFriendlyFontDescription =
                getContext()
                        .getResources()
                        .getString(
                                R.string.font_size_accessibility_label,
                                mFormat.format(mUserFontScaleFactor));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            mSeekBar.setStateDescription(userFriendlyFontDescription);
        } else {
            mSeekBar.setContentDescription(userFriendlyFontDescription);
        }

        mPreview.setTextSize(
                TypedValue.COMPLEX_UNIT_DIP, SMALLEST_STANDARD_FONT_SIZE_PX * mFontScaleFactor);
    }

    /**
     * {@link SeekBar} only supports integer steps, and starts from 0, so the following methods are
     * necessary to scale floating point pref values to/from integer progress amounts.
     *
     * @param progress {@link SeekBar} progress amount.
     * @return User font scale factor preference value.
     */
    private float progressToUserFontScaleFactor(int progress) {
        return MIN + progress * STEP;
    }

    private int userFontScaleFactorToProgress(float userFontScaleFactor) {
        return Math.round((userFontScaleFactor - MIN) / STEP);
    }

    /** Notifies {@link Preference.OnPreferenceChangeListener}s of updated value. */
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (!fromUser) return;

        float userFontScaleFactor = progressToUserFontScaleFactor(progress);
        if (userFontScaleFactor == mUserFontScaleFactor) return;

        callChangeListener(userFontScaleFactor);
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {}

    public CharSequence getAmountForTesting() {
        return mAmount.getText();
    }
}
