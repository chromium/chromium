// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.Collections;

/**
 * Favicon provider for data sharing UI. Wraps around the favicon backend and provides convenient
 * methods for fetching foreign favicon drawable or bitmap for a given URL as required by the client
 * UI.
 */
@NullMarked
public class DataSharingFaviconProvider implements RecentActivityListCoordinator.FaviconProvider {
    private final Context mContext;
    private final BulkFaviconUtil mBulkFaviconUtil;
    private final int mFaviconSize;
    private final Profile mProfile;

    /**
     * Constructor.
     *
     * @param context The associated context.
     * @param profile The associated profile.
     * @param bulkFaviconUtil Utility to fetch favicons.
     */
    public DataSharingFaviconProvider(
            Context context, Profile profile, BulkFaviconUtil bulkFaviconUtil) {
        mContext = context;
        mBulkFaviconUtil = bulkFaviconUtil;
        mFaviconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.recent_activity_favicon_size);
        mProfile = profile;
    }

    @Override
    public void fetchFavicon(GURL tabUrl, Callback<Drawable> faviconDrawableCallback) {
        mBulkFaviconUtil.fetchAsDrawable(
                mContext,
                mProfile,
                Collections.singletonList(tabUrl),
                mFaviconSize,
                (results) -> {
                    faviconDrawableCallback.onResult(results.get(0));
                });
    }

    @Override
    public void destroy() {}
}
