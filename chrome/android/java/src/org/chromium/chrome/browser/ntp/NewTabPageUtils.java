// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Collection of util methods for help launching a NewTabPage. */
@NullMarked
public class NewTabPageUtils {
    /** Padding style options for NTP Aurora. */
    @IntDef({PaddingStyle.DEFAULT, PaddingStyle.SMALL, PaddingStyle.MEDIUM, PaddingStyle.LARGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PaddingStyle {
        int DEFAULT = 0;
        int SMALL = 1;
        int MEDIUM = 2;
        int LARGE = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Updates the margins for the most visited tiles layout.
     *
     * <p>// TODO(crbug.com/481717794): Re-evaluate all vertical gaps on the NTP. The gap between //
     * the Composeplate (or Search Box) and MVT is currently ~25dp, but should likely be // unified
     * and reduced to 16dp in a future UI polish pass.
     */
    public static void updateTilesLayoutTopMargin(
            View view, boolean shouldShowLogo, boolean isLff) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        Resources resources = view.getResources();
        int topMargin =
                resources.getDimensionPixelSize(
                        (shouldShowLogo || isLff)
                                ? R.dimen.ntp_section_top_margin
                                : R.dimen.tile_layout_no_logo_top_margin);

        marginLayoutParams.topMargin = topMargin;
        view.setLayoutParams(marginLayoutParams);
    }

    /** Returns the {@link PaddingStyle} for NTP Aurora. */
    public static @PaddingStyle int getPaddingStyleForAurora() {
        return ChromeFeatureList.sNtpAuroraPaddingStyle.getValue();
    }
}
