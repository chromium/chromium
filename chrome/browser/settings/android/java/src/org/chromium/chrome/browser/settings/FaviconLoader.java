// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.url.GURL;

/**
 * A helper class that groups a FaviconHelper with its corresponding Callback
 * to load favicons for settings views.
 *
 * This object is kept alive by being passed to the native
 * FaviconHelper.getLocalFaviconImageForURL. Its reference will be released after the callback
 * has been called.
 */
public class FaviconLoader implements FaviconHelper.FaviconImageCallback {
    // Constants for favicon processing.
    // TODO(crbug.com/1076571): Move these constants to colors.xml and dimens.xml
    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;
    // Sets the favicon corner radius to 12.5% of favicon size (2dp for a 16dp favicon)
    private static final float FAVICON_CORNER_RADIUS_FRACTION = 0.125f;
    // Sets the favicon text size to 62.5% of favicon size (10dp for a 16dp favicon)
    private static final float FAVICON_TEXT_SIZE_FRACTION = 0.625f;

    private final Resources mResources;
    private final GURL mFaviconUrl;
    private final Callback<Bitmap> mCallback;
    private final int mFaviconSizePx;
    // Loads the favicons asynchronously.
    private final FaviconHelper mFaviconHelper;

    public FaviconLoader(
            Profile profile, Resources resources, GURL faviconUrl, Callback<Bitmap> callback) {
        mResources = resources;
        mFaviconUrl = faviconUrl;
        mCallback = callback;
        mFaviconSizePx = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        mFaviconHelper = new FaviconHelper();

        if (!mFaviconHelper.getLocalFaviconImageForURL(
                    profile, mFaviconUrl, mFaviconSizePx, this)) {
            onFaviconAvailable(/*image=*/null, null);
        }
    }

    @Override
    public void onFaviconAvailable(Bitmap image, GURL unusedIconUrl) {
        mFaviconHelper.destroy();

        if (image == null) {
            // Invalid or no favicon, produce a generic one.
            float density = mResources.getDisplayMetrics().density;
            int faviconSizeDp = Math.round(mFaviconSizePx / density);
            RoundedIconGenerator faviconGenerator =
                    new RoundedIconGenerator(mResources, faviconSizeDp, faviconSizeDp,
                            Math.round(FAVICON_CORNER_RADIUS_FRACTION * faviconSizeDp),
                            FAVICON_BACKGROUND_COLOR,
                            Math.round(FAVICON_TEXT_SIZE_FRACTION * faviconSizeDp));
            image = faviconGenerator.generateIconForUrl(mFaviconUrl);
        }
        mCallback.onResult(image);
    }
}
