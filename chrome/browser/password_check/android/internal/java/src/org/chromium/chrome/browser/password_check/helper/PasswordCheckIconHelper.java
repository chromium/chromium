// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check.helper;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_check.CompromisedCredential;
import org.chromium.chrome.browser.password_check.R;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/** Helper used to fetch or create an appropriate icon for a compromised credential. */
public class PasswordCheckIconHelper {
    /** Data object containing all data required to set an icon or construct a fallback. */
    public static class FaviconOrFallback {
        public final String mUrlOrAppName;
        public final @Nullable Bitmap mIcon;
        public final int mFallbackColor;
        public final boolean mIsFallbackColorDefault;
        public final int mIconType;
        public final int mIconSize;

        FaviconOrFallback(
                String urlOrAppName,
                @Nullable Bitmap icon,
                int fallbackColor,
                boolean isFallbackColorDefault,
                int iconType,
                int iconSize) {
            mUrlOrAppName = urlOrAppName;
            mIcon = icon;
            mFallbackColor = fallbackColor;
            mIsFallbackColorDefault = isFallbackColorDefault;
            mIconType = iconType;
            mIconSize = iconSize;
        }
    }

    private final LargeIconBridge mLargeIconBridge;
    private final int mDesiredIconSize;

    public PasswordCheckIconHelper(LargeIconBridge largeIconBridge, int desiredIconSize) {
        mLargeIconBridge = largeIconBridge;
        mDesiredIconSize = desiredIconSize;
    }

    public void getLargeIcon(
            CompromisedCredential credential, Callback<FaviconOrFallback> iconCallback) {
        final Pair<GURL, String> originAndFallback = getIconOriginAndFallback(credential);
        if (!originAndFallback.first.isValid()) {
            iconCallback.onResult(
                    new FaviconOrFallback(
                            originAndFallback.second,
                            null,
                            0,
                            true,
                            IconType.INVALID,
                            mDesiredIconSize));
            return; // Abort because an invalid icon URLs will crash Chrome!
        }
        mLargeIconBridge.getLargeIconForUrl(
                originAndFallback.first,
                mDesiredIconSize,
                (icon, fallbackColor, hasDefaultColor, type) -> {
                    iconCallback.onResult(
                            new FaviconOrFallback(
                                    originAndFallback.second,
                                    icon,
                                    fallbackColor,
                                    hasDefaultColor,
                                    type,
                                    mDesiredIconSize));
                });
    }

    /**
     * @param faviconOrFallbackData A {@link FaviconOrFallback} containing color or fallback hints.
     * @param context A {@link Context} object used to load default colors if necessary.
     * @return A color (non-resource value) to use for monogram icons.
     */
    public static int getIconColor(
            PasswordCheckIconHelper.FaviconOrFallback faviconOrFallbackData, Context context) {
        return faviconOrFallbackData.mIsFallbackColorDefault
                ? context.getColor(R.color.default_favicon_background_color)
                : faviconOrFallbackData.mFallbackColor;
    }

    /**
     * Determines which origin to use for retrieving a favicon.
     * @param credential A {@link CompromisedCredential}.
     * @return A pair with (potentially invalid) icon origin and a fallback URL for monograms.
     */
    private static Pair<GURL, String> getIconOriginAndFallback(CompromisedCredential credential) {
        if (!credential.getAssociatedApp().isEmpty()) {
            return getIconOriginAndFallbackForApp(credential);
        }

        // Ideally, the sign-on realm is valid and has an adjacent, valid icon.
        GURL iconOrigin = new GURL(credential.getSignonRealm());
        String fallbackUrl = credential.getSignonRealm();
        if (!iconOrigin.isValid()) {
            // If the sign-on realm isn't valid, try the change URL instead.
            iconOrigin = new GURL(credential.getPasswordChangeUrl());
            fallbackUrl = credential.getPasswordChangeUrl();
        }
        if (!iconOrigin.isValid()) {
            // If neither URL is valid, try the display origin as a last, very weak hope.
            iconOrigin = credential.getAssociatedUrl();
            fallbackUrl = credential.getDisplayOrigin();
        }
        return new Pair<>(iconOrigin, fallbackUrl);
    }

    private static Pair<GURL, String> getIconOriginAndFallbackForApp(
            CompromisedCredential credential) {
        return new Pair<>(new GURL(credential.getDisplayOrigin()), credential.getDisplayOrigin());
    }
}
