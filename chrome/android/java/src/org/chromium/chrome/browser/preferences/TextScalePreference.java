// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.accessibility.FontSizePrefs.FontSizePrefsObserver;

/**
 * Preference that allows the user to change the scaling factor that's applied to web page text.
 * This also shows a preview of how large a typical web page's text will appear.
 */
public class TextScalePreference extends SeekBarPreference {
    private TextView mPreview;
    private final FontSizePrefs mFontSizePrefs;

    private final FontSizePrefsObserver mFontSizePrefsObserver = new FontSizePrefsObserver() {
        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            updatePreview();
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {}
    };

    /**
     * Constructor for inflating from XML.
     */
    public TextScalePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mFontSizePrefs = FontSizePrefs.getInstance();

        setLayoutResource(R.layout.custom_preference);
        setWidgetLayoutResource(R.layout.preference_text_scale);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        if (mPreview == null) {
            mPreview = (TextView) holder.findViewById(R.id.preview);
            updatePreview();
        }
    }

    /**
     * Listens for changes to the text scale and updates the preview text as needed. This must be
     * matched with a call to stopObservingFontPrefs().
     */
    public void startObservingFontPrefs() {
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
        updatePreview();
    }

    /**
     * Stops listening for changes to the text scale.
     */
    public void stopObservingFontPrefs() {
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
    }

    private void updatePreview() {
        if (mPreview != null) {
            // Online body text tends to be around 13-16px. We ask the user to adjust the text scale
            // until 12px text is legible, that way all body text will be legible (and since font
            // boosting approximately preserves relative font size differences, other text will be
            // bigger/smaller as appropriate).
            final float smallestStandardWebPageFontSize = 12.0f; // CSS px
            mPreview.setTextSize(TypedValue.COMPLEX_UNIT_DIP,
                    smallestStandardWebPageFontSize * mFontSizePrefs.getFontScaleFactor());
        }
    }

}
