// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.ImageButtonPreference;
import org.chromium.url.GURL;

/**
 * A Preference to represent a site using FLEDGE.
 */
public class FledgePreference extends ImageButtonPreference {
    private static final int FAVICON_PADDING_DP = 4;

    // The ETLD+1 that used Fledge.
    private final @NonNull String mSite;
    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    public FledgePreference(Context context, @NonNull String site) {
        super(context);
        mSite = site;
        setTitle(site);
    }

    @NonNull
    public String getSite() {
        return mSite;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            // Since Fledge is only available in secure contexts, use https as scheme.
            new FaviconLoader(Profile.getLastUsedRegularProfile(), getContext().getResources(),
                    new GURL("https://" + mSite), this::onFaviconAvailable);
            mFaviconFetched = true;
        }

        float density = getContext().getResources().getDisplayMetrics().density;
        int iconPadding = Math.round(FAVICON_PADDING_DP * density);
        View iconView = holder.findViewById(android.R.id.icon);
        iconView.setPadding(iconPadding, iconPadding, iconPadding, iconPadding);
    }

    private void onFaviconAvailable(Bitmap image) {
        if (image != null) {
            setIcon(new BitmapDrawable(getContext().getResources(), image));
        }
    }
}
