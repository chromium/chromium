// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.helper;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/**
 * Provides default favicons and helps to fetch and set favicons.
 */
public class FaviconHelper {
    private final Resources mResources;
    private final RoundedIconGenerator mIconGenerator;
    private final int mDesiredSize;

    /** Factory used to create this helper or a mock in tests. */
    @VisibleForTesting
    public interface CreationStrategy {
        /**
         * Creates a non-null favicon helper or returns a static mock in tests.
         * @return A {@link FaviconHelper}.
         */
        FaviconHelper create(Context context);
    }

    private static CreationStrategy sCreationStrategy = FaviconHelper::new;

    public static FaviconHelper create(Context context) {
        return sCreationStrategy.create(context);
    }

    @VisibleForTesting
    public static void setCreationStrategy(CreationStrategy strategy) {
        sCreationStrategy = strategy;
    }

    /**
     * Creates a new helper.
     * @param context The {@link Context} used to fetch resources and create Drawables.
     */
    protected FaviconHelper(Context context) {
        mResources = context.getResources();
        mDesiredSize =
                mResources.getDimensionPixelSize(R.dimen.keyboard_accessory_suggestion_icon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mResources);
    }

    public Drawable getDefaultIcon(String origin) {
        return FaviconUtils.getIconDrawableWithoutFilter(null, origin,
                R.color.default_favicon_background_color, mIconGenerator, mResources, mDesiredSize);
    }

    /**
     * Resets favicon in case the container is recycled. Then queries a favicon for the origin.
     * @param origin The origin URL of the favicon.
     * @param setIconCallback Callback called with fetched icons. May be called with null.
     */
    public void fetchFavicon(String origin, Callback<Drawable> setIconCallback) {
        final LargeIconBridge mIconBridge =
                new LargeIconBridge(Profile.getLastUsedRegularProfile());
        final GURL gurlOrigin = new GURL(origin);
        if (!gurlOrigin.isValid()) return;
        mIconBridge.getLargeIconForUrl(gurlOrigin, mDesiredSize,
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(
                            icon, origin, fallbackColor, mIconGenerator, mResources, mDesiredSize);
                    setIconCallback.onResult(drawable);
                });
    }
}
