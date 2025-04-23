// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.browser_ui.settings.ImageButtonPreference;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/** A Preference to represent a site using FLEDGE. */
@NullMarked
public class FledgePreference extends ImageButtonPreference {
    // The ETLD+1 that used Fledge.
    private final String mSite;
    private final LargeIconBridge mLargeIconBridge;
    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    public FledgePreference(Context context, String site, LargeIconBridge largeIconBridge) {
        super(context);
        mSite = site;
        mLargeIconBridge = largeIconBridge;
        setTitle(site);
    }

    public String getSite() {
        return mSite;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched) {
            // Since Fledge is only available in secure contexts, use https as scheme.
            FaviconLoader.loadFavicon(
                    getContext(),
                    mLargeIconBridge,
                    new GURL("https://" + mSite),
                    this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }
}
