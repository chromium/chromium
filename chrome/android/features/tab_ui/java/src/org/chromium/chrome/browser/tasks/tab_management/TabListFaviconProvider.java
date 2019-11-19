// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconUtils;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.tab_ui.R;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {
    private static Drawable sRoundedGlobeDrawable;
    private static Drawable sRoundedChromeDrawable;
    private final int mFaviconSize;
    private final Profile mProfile;
    private final FaviconHelper mFaviconHelper;
    private final Context mContext;
    @ColorInt
    private final int mDefaultIconColor;
    @ColorInt
    private final int mIncognitoIconColor;

    /**
     * Construct the provider that provides favicons for tab list.
     * @param context The context to use for accessing {@link android.content.res.Resources}
     * @param profile The profile to use for getting favicons.
     */
    public TabListFaviconProvider(Context context, Profile profile) {
        mContext = context;
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mProfile = profile;
        mFaviconHelper = new FaviconHelper();
        if (sRoundedGlobeDrawable == null) {
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            Bitmap globeBitmap =
                    Bitmap.createBitmap(mFaviconSize, mFaviconSize, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(globeBitmap);
            globeDrawable.setBounds(0, 0, mFaviconSize, mFaviconSize);
            globeDrawable.draw(canvas);
            sRoundedGlobeDrawable = processBitmap(globeBitmap);
        }
        if (sRoundedChromeDrawable == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeDrawable = processBitmap(chromeBitmap);
        }
        mDefaultIconColor = mContext.getResources().getColor(R.color.default_icon_color);
        mIncognitoIconColor = mContext.getResources().getColor(R.color.default_icon_color_white);
    }

    private Drawable processBitmap(Bitmap bitmap) {
        return FaviconUtils.createRoundedBitmapDrawable(mContext.getResources(),
                Bitmap.createScaledBitmap(bitmap, mFaviconSize, mFaviconSize, true));
    }

    /**
     * @return The scaled rounded Globe Drawable as default favicon.
     * @param isIncognito Whether the {@link Drawable} is used for incognito mode.
     */
    public Drawable getDefaultFaviconDrawable(boolean isIncognito) {
        return getRoundedGlobeDrawable(isIncognito);
    }

    /**
     * Asynchronously get the processed favicon Drawable.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    public void getFaviconForUrlAsync(
            String url, boolean isIncognito, Callback<Drawable> faviconCallback) {
        if (NativePageFactory.isNativePageUrl(url, isIncognito)) {
            faviconCallback.onResult(getRoundedChromeDrawable(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, url, mFaviconSize, (image, iconUrl) -> {
                        if (image == null) {
                            faviconCallback.onResult(getRoundedGlobeDrawable(isIncognito));
                        } else {
                            faviconCallback.onResult(processBitmap(image));
                        }
                    });
        }
    }

    /**
     * Synchronously get the processed favicon Drawable.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param icon The favicon that was received.
     * @return The processed favicon.
     */
    public Drawable getFaviconForUrlSync(String url, boolean isIncognito, Bitmap icon) {
        if (icon == null) {
            boolean isNativeUrl = NativePageFactory.isNativePageUrl(url, isIncognito);
            return isNativeUrl ? getRoundedChromeDrawable(isIncognito)
                               : getRoundedGlobeDrawable(isIncognito);
        } else {
            return processBitmap(icon);
        }
    }

    private Drawable getRoundedChromeDrawable(boolean isIncognito) {
        @ColorInt
        int color = isIncognito ? mIncognitoIconColor : mDefaultIconColor;
        // Since static variable is still loaded when activity is destroyed due to configuration
        // changes, e.g. light/dark theme changes, setColorFilter is needed when we retrieve the
        // drawable. setColorFilter would be a no-op if color and the mode are the same.
        sRoundedChromeDrawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        return sRoundedChromeDrawable;
    }

    private Drawable getRoundedGlobeDrawable(boolean isIncognito) {
        @ColorInt
        int color = isIncognito ? mIncognitoIconColor : mDefaultIconColor;
        // Since static variable is still loaded when activity is destroyed due to configuration
        // changes, e.g. light/dark theme changes, setColorFilter is needed when we retrieve the
        // drawable. setColorFilter would be a no-op if color and the mode are the same.
        sRoundedGlobeDrawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        return sRoundedGlobeDrawable;
    }
}
