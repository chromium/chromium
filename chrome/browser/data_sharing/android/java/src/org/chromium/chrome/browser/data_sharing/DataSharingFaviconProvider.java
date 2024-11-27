// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.url.GURL;

/**
 * Favicon provider for data sharing UI. Wraps around the favicon backend and provides convenient
 * methods for fetching foreign favicon drawable or bitmap for a given URL as required by the client
 * UI.
 */
public class DataSharingFaviconProvider implements RecentActivityListCoordinator.FaviconProvider {
    private final Context mContext;
    private final FaviconHelper mFaviconHelper;
    private final DefaultFaviconHelper mDefaultFaviconHelper;
    private final RoundedIconGenerator mRounedIconGenerator;
    private final int mFaviconSize;
    private final Profile mProfile;

    /**
     * Constructor.
     *
     * @param context The associated context.
     * @param profile The associated profile.
     * @param faviconHelper The backend that provides favicons.
     */
    public DataSharingFaviconProvider(
            Context context, Profile profile, FaviconHelper faviconHelper) {
        mContext = context;
        mFaviconHelper = faviconHelper;
        mDefaultFaviconHelper = new DefaultFaviconHelper();
        mRounedIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mFaviconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
        mProfile = profile;
    }

    @Override
    public void fetchFavicon(GURL tabUrl, Callback<Drawable> faviconDrawableCallback) {
        FaviconImageCallback imageCallback =
                (bitmap, iconUrl) -> {
                    Drawable faviconDrawable =
                            FaviconUtils.getIconDrawableWithFilter(
                                    bitmap,
                                    tabUrl,
                                    mRounedIconGenerator,
                                    mDefaultFaviconHelper,
                                    mContext,
                                    mFaviconSize);
                    faviconDrawableCallback.onResult(faviconDrawable);
                };

        mFaviconHelper.getForeignFaviconImageForURL(mProfile, tabUrl, mFaviconSize, imageCallback);
    }

    @Override
    public void destroy() {
        mFaviconHelper.destroy();
    }
}
