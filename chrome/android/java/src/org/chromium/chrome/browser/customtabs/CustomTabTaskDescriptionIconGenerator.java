// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;

/**
 * Generates icons suitable for Custom Tabs in the recent tasks list.
 */
public class CustomTabTaskDescriptionIconGenerator {
    private static final int APP_ICON_MIN_SIZE_DP = 32;
    private static final int APP_ICON_SIZE_DP = 64;
    private static final int APP_ICON_CORNER_RADIUS_DP = 3;
    private static final int APP_ICON_TEXT_SIZE_DP = 30;
    private static final int APP_ICON_DEFAULT_BACKGROUND_COLOR = 0xFF323232;

    private Context mContext;
    private int mMinSizePx;

    /**
     * The page URL for which {@link #mGeneratedIcon} was generated.
     */
    private String mGeneratedPageUrl;

    /**
     * The most recently generated icon.
     */
    private Bitmap mGeneratedIcon;

    /**
     * Generates the icon if there is no adequate favicon.
     */
    private RoundedIconGenerator mGenerator;

    public CustomTabTaskDescriptionIconGenerator(Context context) {
        mContext = context;
        mMinSizePx =
                (int) mContext.getResources().getDisplayMetrics().density * APP_ICON_MIN_SIZE_DP;
    }

    /**
     * Returns the icon to use for the Activity in the recent tasks list. Returns the favicon if it
     * is adequate. If the passed in favicon is not adequate, an icon is generated from the
     * page URL.
     *
     * @param pageUrl The URL of the tab.
     * @param largestFavicon The largest favicon available at the page URL.
     * @return The icon to use in the recent tasks list.
     */
    public Bitmap getBitmap(String pageUrl, Bitmap largestFavicon) {
        if (largestFavicon != null && largestFavicon.getWidth() >= mMinSizePx
                && largestFavicon.getHeight() >= mMinSizePx) {
            return largestFavicon;
        }

        if (TextUtils.equals(pageUrl, mGeneratedPageUrl)) {
            return mGeneratedIcon;
        }

        if (mGenerator == null) {
            mGenerator = new RoundedIconGenerator(mContext.getResources(), APP_ICON_SIZE_DP,
                    APP_ICON_SIZE_DP, APP_ICON_CORNER_RADIUS_DP, APP_ICON_DEFAULT_BACKGROUND_COLOR,
                    APP_ICON_TEXT_SIZE_DP);
        }

        mGeneratedPageUrl = pageUrl;
        mGeneratedIcon = mGenerator.generateIconForUrl(pageUrl);
        return mGeneratedIcon;
    }
}
