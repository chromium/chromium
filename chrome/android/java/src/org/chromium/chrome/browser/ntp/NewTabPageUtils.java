// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.res.Resources;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Collection of util methods for help launching a NewTabPage. */
@NullMarked
public class NewTabPageUtils {
    private static final String ORIGIN_PARAMETER_KEY = "origin";
    private static final String WEB_FEED_PARAMETER = "web-feed";

    /**
     * @return The NTP url encoded with {@link NewTabPageLaunchOrigin} information.
     */
    public static String encodeNtpUrl(Profile profile, @NewTabPageLaunchOrigin int launchOrigin) {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(profile);
        Uri.Builder uriBuilder = Uri.parse(resolver.getNtpUrl()).buildUpon();
        switch (launchOrigin) {
            case NewTabPageLaunchOrigin.WEB_FEED:
                uriBuilder.appendQueryParameter(ORIGIN_PARAMETER_KEY, WEB_FEED_PARAMETER);
                break;
            case NewTabPageLaunchOrigin.UNKNOWN:
            default:
                break;
        }
        return uriBuilder.build().toString();
    }

    /**
     * @return The {@link NewTabPageLaunchOrigin} decoded from the NTP url.
     */
    public static @NewTabPageLaunchOrigin int decodeOriginFromNtpUrl(String url) {
        if (!UrlUtilities.isNtpUrl(url)) {
            return NewTabPageLaunchOrigin.UNKNOWN;
        }
        Uri uri = Uri.parse(url);
        String origin = uri.getQueryParameter(ORIGIN_PARAMETER_KEY);
        if (origin != null && origin.equals(WEB_FEED_PARAMETER)) {
            return NewTabPageLaunchOrigin.WEB_FEED;
        }
        return NewTabPageLaunchOrigin.UNKNOWN;
    }

    /**
     * Updates the margins for the most visited tiles layout.
     *
     * <p>// TODO(crbug.com/481717794): Re-evaluate all vertical gaps on the NTP. The gap between //
     * the Composeplate (or Search Box) and MVT is currently ~25dp, but should likely be // unified
     * and reduced to 16dp in a future UI polish pass.
     */
    public static void updateTilesLayoutTopMargin(
            View view, boolean shouldShowLogo, boolean isTablet) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        Resources resources = view.getResources();
        int topMargin =
                resources.getDimensionPixelSize(
                        (shouldShowLogo || isTablet)
                                ? R.dimen.ntp_section_top_margin
                                : R.dimen.tile_layout_no_logo_top_margin);

        marginLayoutParams.topMargin = topMargin;
        view.setLayoutParams(marginLayoutParams);
    }
}
