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
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;

import java.util.List;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {
    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;

    private static Drawable sRoundedGlobeDrawable;
    private static Drawable sRoundedGlobeDrawableForStrip;
    private static Drawable sRoundedChromeDrawable;
    private static Drawable sRoundedChromeDrawableForStrip;
    private static Drawable sRoundedComposedDefaultDrawable;
    private final int mStripFaviconSize;
    private final int mDefaultFaviconSize;
    private final int mFaviconSize;
    private final int mFaviconInset;
    private final boolean mIsTabStrip;
    private final Context mContext;
    @ColorInt
    private final int mDefaultIconColor;
    @ColorInt
    private final int mIncognitoIconColor;
    private boolean mIsInitialized;

    private Profile mProfile;
    private FaviconHelper mFaviconHelper;

    /**
     * Construct the provider that provides favicons for tab list.
     * @param context    The context to use for accessing {@link android.content.res.Resources}
     * @param isTabStrip Indicator for whether this class provides favicons for tab strip or not.
     *
     */
    public TabListFaviconProvider(Context context, boolean isTabStrip) {
        mContext = context;
        mDefaultFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mStripFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_size);
        mFaviconSize = isTabStrip ? mStripFaviconSize : mDefaultFaviconSize;
        mFaviconInset = ViewUtils.dpToPx(context,
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_inset));
        mIsTabStrip = isTabStrip;

        if (sRoundedGlobeDrawable == null) {
            // TODO(crbug.com/1066709): From Android Developer Documentation, we should avoid
            //  resizing vector drawable.
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            sRoundedGlobeDrawable = processBitmap(
                    getResizedBitmapFromDrawable(globeDrawable, mDefaultFaviconSize), false);
        }
        if (sRoundedGlobeDrawableForStrip == null) {
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            sRoundedGlobeDrawableForStrip = processBitmap(
                    getResizedBitmapFromDrawable(globeDrawable, mStripFaviconSize), true);
        }
        if (sRoundedChromeDrawable == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeDrawable = processBitmap(chromeBitmap, false);
        }
        if (sRoundedChromeDrawableForStrip == null) {
            Drawable chromeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.chromelogo16);
            sRoundedChromeDrawableForStrip = processBitmap(
                    getResizedBitmapFromDrawable(chromeDrawable, mStripFaviconSize), true);
        }
        if (sRoundedComposedDefaultDrawable == null) {
            sRoundedComposedDefaultDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp);
        }
        mDefaultIconColor = mContext.getResources().getColor(R.color.default_icon_color);
        mIncognitoIconColor = mContext.getResources().getColor(R.color.default_icon_color_light);
    }

    public void initWithNative(Profile profile) {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mProfile = profile;
        mFaviconHelper = new FaviconHelper();
    }

    public boolean isInitialized() {
        return mIsInitialized;
    }

    private Bitmap getResizedBitmapFromDrawable(Drawable drawable, int size) {
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, size, size);
        drawable.draw(canvas);
        return bitmap;
    }

    private Drawable processBitmap(Bitmap bitmap, boolean isTabStrip) {
        int size = isTabStrip ? mStripFaviconSize : mDefaultFaviconSize;
        Drawable favicon = FaviconUtils.createRoundedBitmapDrawable(
                mContext.getResources(), Bitmap.createScaledBitmap(bitmap, size, size, true));
        if (!isTabStrip) {
            return favicon;
        }
        Drawable circleBackground =
                AppCompatResources.getDrawable(mContext, R.drawable.tab_strip_favicon_circle);
        Drawable[] layers = {circleBackground, favicon};
        LayerDrawable layerDrawable = new LayerDrawable(layers);
        layerDrawable.setLayerInset(1, mFaviconInset, mFaviconInset, mFaviconInset, mFaviconInset);
        return layerDrawable;
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
        if (mFaviconHelper == null || UrlUtilities.isNTPUrl(url)) {
            faviconCallback.onResult(getRoundedChromeDrawable(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, url, mFaviconSize, (image, iconUrl) -> {
                        if (image == null) {
                            faviconCallback.onResult(getRoundedGlobeDrawable(isIncognito));
                        } else {
                            faviconCallback.onResult(processBitmap(image, mIsTabStrip));
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
            boolean isNativeUrl = NativePage.isNativePageUrl(url, isIncognito);
            return isNativeUrl ? getRoundedChromeDrawable(isIncognito)
                               : getRoundedGlobeDrawable(isIncognito);
        } else {
            return processBitmap(icon, mIsTabStrip);
        }
    }

    /**
     * Asynchronously get the composed, up to 4, favicon Drawable.
     * @param urls List of urls, up to 4, whose favicon are requested to be composed.
     * @param isIncognito Whether the processed composed favicon is used for incognito or not.
     * @param faviconCallback The callback that requests for the composed favicon.
     */
    public void getComposedFaviconImageAsync(
            List<String> urls, boolean isIncognito, Callback<Drawable> faviconCallback) {
        assert urls != null && urls.size() > 1 && urls.size() <= 4;

        mFaviconHelper.getComposedFaviconImage(mProfile, urls, mFaviconSize, (image, iconUrl) -> {
            if (image == null) {
                faviconCallback.onResult(getDefaultComposedImage(isIncognito));
            } else {
                faviconCallback.onResult(processBitmap(image, mIsTabStrip));
            }
        });
    }

    private Drawable getDefaultComposedImage(boolean isIncognito) {
        return getTintedDrawable(sRoundedComposedDefaultDrawable, isIncognito);
    }

    private Drawable getRoundedChromeDrawable(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedChromeDrawableForStrip;
        }
        return getTintedDrawable(sRoundedChromeDrawable, isIncognito);
    }

    private Drawable getRoundedGlobeDrawable(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedGlobeDrawableForStrip;
        }
        return getTintedDrawable(sRoundedGlobeDrawable, isIncognito);
    }

    private Drawable getTintedDrawable(Drawable drawable, boolean isIncognito) {
        @ColorInt
        int color = isIncognito ? mIncognitoIconColor : mDefaultIconColor;
        // Since static variable is still loaded when activity is destroyed due to configuration
        // changes, e.g. light/dark theme changes, setColorFilter is needed when we retrieve the
        // drawable. setColorFilter would be a no-op if color and the mode are the same.
        drawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        return drawable;
    }
}
