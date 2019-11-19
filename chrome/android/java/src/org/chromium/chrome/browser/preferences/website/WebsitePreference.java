// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.text.format.Formatter;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.preferences.ChromeImageViewPreference;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;

/**
 * A preference that displays a website's favicon and URL and, optionally, the amount of local
 * storage used by the site. This preference can also display an additional icon on the right side
 * of the preference. See {@link ChromeImageViewPreference} for more details on how this icon
 * can be used.
 */
class WebsitePreference extends ChromeImageViewPreference implements FaviconImageCallback {
    private final Website mSite;
    private final SiteSettingsCategory mCategory;

    private static final int TEXT_SIZE_SP = 13;

    // Loads the favicons asynchronously.
    private FaviconHelper mFaviconHelper;

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    // Metrics for favicon processing.
    // Sets the favicon corner radius to 12.5% of favicon size (2dp for a 16dp favicon)
    private static final float FAVICON_CORNER_RADIUS_FRACTION = 0.125f;
    private static final int FAVICON_PADDING_DP = 4;
    // Sets the favicon text size to 62.5% of favicon size (10dp for a 16dp favicon)
    private static final float FAVICON_TEXT_SIZE_FRACTION = 0.625f;
    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;

    private int mFaviconSizePx;

    WebsitePreference(Context context, Website site, SiteSettingsCategory category) {
        super(context);
        mSite = site;
        mCategory = category;
        setWidgetLayoutResource(R.layout.website_features);
        mFaviconSizePx = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));

        refresh();
    }

    public void putSiteIntoExtras(String key) {
        getExtras().putSerializable(key, mSite);
    }

    public void putSiteAddressIntoExtras(String key) {
        getExtras().putSerializable(key, mSite.getAddress());
    }

    /**
     * Return the Website this object is representing.
     */
    public Website site() {
        return mSite;
    }

    @Override
    public void onFaviconAvailable(Bitmap image, String iconUrl) {
        mFaviconHelper.destroy();
        mFaviconHelper = null;
        Resources resources = getContext().getResources();
        if (image == null) {
            // Invalid favicon, produce a generic one.
            float density = resources.getDisplayMetrics().density;
            int faviconSizeDp = Math.round(mFaviconSizePx / density);
            RoundedIconGenerator faviconGenerator =
                    new RoundedIconGenerator(resources, faviconSizeDp, faviconSizeDp,
                            Math.round(FAVICON_CORNER_RADIUS_FRACTION * faviconSizeDp),
                            FAVICON_BACKGROUND_COLOR,
                            Math.round(FAVICON_TEXT_SIZE_FRACTION * faviconSizeDp));
            image = faviconGenerator.generateIconForUrl(faviconUrl());
        }

        setIcon(new BitmapDrawable(resources, image));
    }

    /**
     * Returns the url of the site to fetch a favicon for.
     */
    private String faviconUrl() {
        String origin = mSite.getAddress().getOrigin();
        Uri uri = Uri.parse(origin);
        if (uri.getPort() != -1) {
            // Remove the port.
            uri = uri.buildUpon().authority(uri.getHost()).build();
        }
        return uri.toString();
    }

    private void refresh() {
        setTitle(mSite.getTitle());
        String subtitleText = mSite.getSummary();
        if (subtitleText != null) {
            setSummary(String.format(getContext().getString(R.string.website_settings_embedded_in),
                                     subtitleText));
        }
    }

    @Override
    public int compareTo(Preference preference) {
        if (!(preference instanceof WebsitePreference)) {
            return super.compareTo(preference);
        }
        WebsitePreference other = (WebsitePreference) preference;
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            return mSite.compareByStorageTo(other.mSite);
        }

        return mSite.compareByAddressTo(other.mSite);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView usageText = (TextView) holder.findViewById(R.id.usage_text);
        usageText.setVisibility(View.GONE);
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            long totalUsage = mSite.getTotalUsage();
            if (totalUsage > 0) {
                usageText.setText(Formatter.formatShortFileSize(getContext(), totalUsage));
                usageText.setTextSize(TEXT_SIZE_SP);
                usageText.setVisibility(View.VISIBLE);
            }
        }

        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mFaviconHelper = new FaviconHelper();
            if (!mFaviconHelper.getLocalFaviconImageForURL(
                        Profile.getLastUsedProfile(), faviconUrl(), mFaviconSizePx, this)) {
                onFaviconAvailable(null, null);
            }
            mFaviconFetched = true;
        }

        float density = getContext().getResources().getDisplayMetrics().density;
        int iconPadding = Math.round(FAVICON_PADDING_DP * density);
        View iconView = holder.findViewById(android.R.id.icon);
        iconView.setPadding(iconPadding, iconPadding, iconPadding, iconPadding);
    }
}
