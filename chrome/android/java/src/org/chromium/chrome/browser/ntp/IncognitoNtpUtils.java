// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;

/** Collection of util methods for help analyzing an Incognito New Tab Page layout. */
@NullMarked
public class IncognitoNtpUtils {

    /** Data class holding metrics for the Incognito NTP layout. */
    @NullMarked
    public static class IncognitoNtpContentMetrics {
        public final double ntpViewHeightPx;
        public final double textContentHeightPx;
        public final double textContentTopPaddingPx;
        public final double textContentBottomPaddingPx;

        public IncognitoNtpContentMetrics(
                double ntpViewHeightPx,
                double textContentHeightPx,
                double textContentTopPaddingPx,
                double textContentBottomPaddingPx) {
            this.ntpViewHeightPx = ntpViewHeightPx;
            this.textContentHeightPx = textContentHeightPx;
            this.textContentTopPaddingPx = textContentTopPaddingPx;
            this.textContentBottomPaddingPx = textContentBottomPaddingPx;
        }
    }

    /**
     * Provides the primary content view of the Incognito New Tab Page for a given tab.
     *
     * @param tab The tab to get the NTP view from.
     * @return The content {@link View} of the Incognito NTP, or {@code null} if it cannot be found.
     */
    public static @Nullable View getIncognitoNtpView(Tab tab) {
        if (tab == null || tab.getView() == null) {
            return null;
        }

        return tab.getView().findViewById(R.id.new_tab_incognito_container);
    }

    /**
     * Calculates metrics of the main text content area on the Incognito New Tab Page.
     *
     * @param ntpView The Incognito NTP view.
     * @return The {@link IncognitoNtpContentMetrics} of the content.
     */
    public static IncognitoNtpContentMetrics getIncognitoNtpContentMetrics(View ntpView) {
        double ntpViewHeightPx = ntpView.getHeight();
        IncognitoNtpContentMetrics emptyMetrics =
                new IncognitoNtpContentMetrics(ntpViewHeightPx, 0, 0, 0);

        if (!(ntpView instanceof ViewGroup)) {
            return emptyMetrics;
        }

        @Nullable int[] textContentBoundaries = getTextContentScreenBoundaries((ViewGroup) ntpView);

        if (textContentBoundaries == null) {
            return emptyMetrics;
        }

        int textContentTopY = textContentBoundaries[0];
        int textContentBottomY = textContentBoundaries[1];

        int[] containerLocation = new int[2];
        ntpView.getLocationOnScreen(containerLocation);
        int ntpViewTopY = containerLocation[1];

        double textContentTopPaddingPx = Math.max(0, textContentTopY - ntpViewTopY);
        double textContentBottomPaddingPx =
                Math.max(0, (ntpViewTopY + ntpViewHeightPx) - textContentBottomY);
        double textContentHeightPx = Math.max(0, textContentBottomY - textContentTopY);

        return new IncognitoNtpContentMetrics(
                ntpViewHeightPx,
                textContentHeightPx,
                textContentTopPaddingPx,
                textContentBottomPaddingPx);
    }

    /**
     * Recursively finds all visible {@link TextView}s within a {@link ViewGroup} and returns the
     * top-most and bottom-most vertical boundaries of their combined content area in screen
     * coordinates.
     *
     * @param viewGroup The {@link ViewGroup} to search within.
     * @return A two-element array where the top-most on-screen Y coordinate is the first element
     *     and the bottom-most is the second. Returns {@code null} if no visible text views are
     *     found.
     */
    private static @Nullable int[] getTextContentScreenBoundaries(ViewGroup viewGroup) {
        int[] boundaries = new int[] {Integer.MAX_VALUE, Integer.MIN_VALUE};
        boolean foundTextView = false;
        int[] textViewLocation = new int[2];

        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);

            if (child instanceof TextView) {
                TextView textView = (TextView) child;
                if (textView.getVisibility() != View.VISIBLE || textView.getText().length() == 0) {
                    continue;
                }
                foundTextView = true;
                textView.getLocationOnScreen(textViewLocation);
                int viewTopOnScreen = textViewLocation[1];
                int viewBottomOnScreen = viewTopOnScreen + textView.getHeight();

                boundaries[0] = Math.min(boundaries[0], viewTopOnScreen);
                boundaries[1] = Math.max(boundaries[1], viewBottomOnScreen);
            } else if (child instanceof ViewGroup) {
                int[] childBoundaries = getTextContentScreenBoundaries((ViewGroup) child);
                if (childBoundaries != null) {
                    foundTextView = true;
                    boundaries[0] = Math.min(boundaries[0], childBoundaries[0]);
                    boundaries[1] = Math.max(boundaries[1], childBoundaries[1]);
                }
            }
        }
        return foundTextView ? boundaries : null;
    }
}
