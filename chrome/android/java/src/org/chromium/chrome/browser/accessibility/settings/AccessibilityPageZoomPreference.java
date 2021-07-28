// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;

/**
 * Preference that allows a user to change the page zoom factor for web contents.
 */
// TODO(mschillaci): This is a placeholder page visible only behind a flag, not finalized UI.
public class AccessibilityPageZoomPreference extends Preference {
    // Available zoom factors for any page.
    private static final float[] AVAILABLE_ZOOM_FACTORS =
            new float[] {0.10f, 0.25f, 0.33f, 0.50f, 0.67f, 0.75f, 0.90f, 1.00f, 1.10f, 1.25f,
                    1.33f, 1.50f, 1.75f, 2.00f, 2.50f, 3.00f, 4.00f, 5.00f};

    // Default index for zoom factor, set to be 100%.
    private static final int DEFAULT_ZOOM_FACTOR_INDEX = 7;

    // Current zoom factor set by the user.
    private int mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;

    private TextView mPageZoomText;
    private ImageButton mDecreaseZoomButton;
    private ImageButton mIncreaseZoomButton;
    private Button mResetZoomButton;

    /**
     * Constructor for inflating from XML.
     */
    public AccessibilityPageZoomPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Bind views and set click listeners.
        mPageZoomText = (TextView) holder.findViewById(
                R.id.accessibility_page_zoom_current_zoom_level_text);
        mDecreaseZoomButton = (ImageButton) holder.findViewById(
                R.id.accessibility_page_zoom_decrease_zoom_button);
        mIncreaseZoomButton = (ImageButton) holder.findViewById(
                R.id.accessibility_page_zoom_increase_zoom_button);
        mResetZoomButton =
                (Button) holder.findViewById(R.id.accessibility_page_zoom_reset_zoom_button);

        mIncreaseZoomButton.setOnClickListener(view -> {
            assert canIncreaseZoom();
            ++mZoomIndex;
            updateTextAndButtonStates();
        });

        mDecreaseZoomButton.setOnClickListener(view -> {
            assert canDecreaseZoom();
            --mZoomIndex;
            updateTextAndButtonStates();
        });

        mResetZoomButton.setOnClickListener(view -> {
            mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;
            updateTextAndButtonStates();
        });

        // Set text on first load.
        updateTextAndButtonStates();
    }

    // Helper method to update the text of the zoom factor and button states after user actions.
    private void updateTextAndButtonStates() {
        mPageZoomText.setText(generateTextFromZoomFactor());
        mIncreaseZoomButton.setEnabled(canIncreaseZoom());
        mDecreaseZoomButton.setEnabled(canDecreaseZoom());
    }

    // Helper method to construct text of the zoom factor after user actions.
    private String generateTextFromZoomFactor() {
        return getContext().getResources().getString(R.string.accessibility_page_zoom_factor,
                (int) (100 * AVAILABLE_ZOOM_FACTORS[mZoomIndex]));
    }

    // Helper method to determine if user can increase zoom.
    private boolean canIncreaseZoom() {
        return mZoomIndex < AVAILABLE_ZOOM_FACTORS.length - 1;
    }

    // Helper method to determine if user can decrease zoom.
    private boolean canDecreaseZoom() {
        return mZoomIndex > 0;
    }
}