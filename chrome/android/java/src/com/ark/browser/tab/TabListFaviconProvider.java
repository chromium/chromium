// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import com.android.launcher3.graphics.ColorExtractor;
import com.zpj.skin.SkinEngine;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {

    private static final String TAG = "TabListFaviconProvider";

    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;

    /**
     * Wrapper class that holds a favicon drawable and whether recolor is allowed.
     */
    public static class TabFavicon {
        private final @NonNull Drawable mDefaultDrawable;
        private final @NonNull Drawable mSelectedDrawable;
        private final boolean mIsRecolorAllowed;
        private final int mColor;

        private TabFavicon(@NonNull Drawable defaultDrawable, int color) {
            this(defaultDrawable, defaultDrawable, false, color);
        }

        private TabFavicon(@NonNull Drawable defaultDrawable, @NonNull Drawable selectedDrawable,
                           boolean allowRecolor, int color) {
            mDefaultDrawable = defaultDrawable;
            mSelectedDrawable = selectedDrawable;
            mIsRecolorAllowed = allowRecolor;
            mColor = color;
        }

        public int getDominantColor() {
            return mColor;
        }

        /** Return the {@link Drawable} when this favicon is not selected */
        public Drawable getDefaultDrawable() {
            return mDefaultDrawable;
        }

        /** Return the {@link Drawable} when this favicon is selected. */
        public Drawable getSelectedDrawable() {
            return mSelectedDrawable;
        }

        /** Return whether this {@link TabFavicon} has a different drawable when selected. */
        public boolean hasSelectedState() {
            return mDefaultDrawable != mSelectedDrawable;
        }

        /** Return whether the drawables this {@link TabFavicon} contains can be recolored. */
        public boolean isRecolorAllowed() {
            return mIsRecolorAllowed;
        }
    }

    private static TabFavicon sRoundedGlobeFavicon;
    private static TabFavicon sRoundedChromeFavicon;
    private static TabFavicon sRoundedComposedDefaultFavicon;

    private static TabFavicon sRoundedGlobeFaviconIncognito;
    private static TabFavicon sRoundedChromeFaviconIncognito;
    private static TabFavicon sRoundedComposedDefaultFaviconIncognito;

    private static TabFavicon sRoundedGlobeFaviconForStrip;
    private static TabFavicon sRoundedChromeFaviconForStrip;

    private final @ColorInt int mDefaultIconColor;
    private final @ColorInt int mSelectedIconColor;
    private final @ColorInt int mIncognitoIconColor;
    private final @ColorInt int mIncognitoSelectedIconColor;

    private final int mStripFaviconSize;
    private final int mDefaultFaviconSize;
    private final int mFaviconSize;
    private final int mFaviconInset;
    private final boolean mIsTabStrip;
    private final Context mContext;
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
                context.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        mStripFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_size);
        mFaviconSize = isTabStrip ? mStripFaviconSize : mDefaultFaviconSize;
        mFaviconInset = ViewUtils.dpToPx(context,
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_inset));
        mIsTabStrip = isTabStrip;

        mDefaultIconColor = getTitleTextColor(context, false, false);
        mSelectedIconColor = getTitleTextColor(context, false, true);
        mIncognitoIconColor = getTitleTextColor(context, true, false);
        mIncognitoSelectedIconColor = getTitleTextColor(context, true, true);

        if (sRoundedGlobeFavicon == null) {
            // TODO(crbug.com/1066709): From Android Developer Documentation, we should avoid
            //  resizing vector drawable.
            Bitmap globeBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp),
                    mDefaultFaviconSize);
            sRoundedGlobeFavicon = createChromeOwnedTabFavicon(
                    globeBitmap, mDefaultIconColor, mSelectedIconColor, false);
        }
        if (sRoundedChromeFavicon == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeFavicon = createChromeOwnedTabFavicon(
                    chromeBitmap, mDefaultIconColor, mSelectedIconColor, false);
        }
        if (sRoundedComposedDefaultFavicon == null) {
            Bitmap composedBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp),
                    mDefaultFaviconSize);
            sRoundedComposedDefaultFavicon = createChromeOwnedTabFavicon(
                    composedBitmap, mDefaultIconColor, mSelectedIconColor, false);
        }
        if (sRoundedGlobeFaviconIncognito == null) {
            Bitmap globeBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp),
                    mDefaultFaviconSize);
            sRoundedGlobeFaviconIncognito = createChromeOwnedTabFavicon(
                    globeBitmap, mIncognitoIconColor, mIncognitoSelectedIconColor, false);
        }
        if (sRoundedChromeFaviconIncognito == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeFaviconIncognito = createChromeOwnedTabFavicon(
                    chromeBitmap, mIncognitoIconColor, mIncognitoSelectedIconColor, false);
        }
        if (sRoundedComposedDefaultFaviconIncognito == null) {
            Bitmap composedBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp),
                    mDefaultFaviconSize);
            sRoundedComposedDefaultFaviconIncognito = createChromeOwnedTabFavicon(
                    composedBitmap, mIncognitoIconColor, mIncognitoSelectedIconColor, false);
        }

        // Tab strip favicons do not recolor when selected.
        if (sRoundedGlobeFaviconForStrip == null) {
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            Bitmap bitmap = getResizedBitmapFromDrawable(globeDrawable, mStripFaviconSize);
            sRoundedGlobeFaviconForStrip = new TabFavicon(processBitmap(
                    bitmap, true), ColorExtractor.findDominantColorByHue(bitmap));
        }
        if (sRoundedChromeFaviconForStrip == null) {
            Drawable chromeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.chromelogo16);
            Bitmap bitmap = getResizedBitmapFromDrawable(chromeDrawable, mStripFaviconSize);
            sRoundedChromeFaviconForStrip = new TabFavicon(processBitmap(bitmap, true),
                    ColorExtractor.findDominantColorByHue(bitmap));
        }
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
     * Returns the scaled rounded globe drawable used for default favicon. Used when favicon is
     * static and not changing colors when its parent component is selected.
     * @see #getDefaultFavicon(boolean)
     */
    public Drawable getDefaultFaviconDrawable(boolean isIncognito) {
        return getDefaultFavicon(isIncognito).getDefaultDrawable();
    }

    /**
     * @return The scaled rounded Globe {@link TabFavicon} as default favicon.
     * @param isIncognito Whether the {@link TabFavicon} is used for incognito mode.
     */
    TabFavicon getDefaultFavicon(boolean isIncognito) {
        return getRoundedGlobeFavicon(isIncognito);
    }

    /**
     * Asynchronously get the processed {@link Drawable}. Used when favicon is static and not
     * changing colors when its parent component is selected.
     * @see #getFaviconForUrlAsync(GURL, boolean, Callback)
     */
    public void getFaviconDrawableForUrlAsync(
            GURL url, boolean isIncognito, Callback<Drawable> faviconCallback) {
        getFaviconForUrlAsync(url, isIncognito,
                tabFavicon -> faviconCallback.onResult(tabFavicon.getDefaultDrawable()));
    }

    public void getFaviconBitmapForUrlAsync(
            GURL url, boolean isIncognito, Callback<Bitmap> faviconCallback) {
        if (mFaviconHelper == null || UrlUtilities.isNTPUrl(url)) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            faviconCallback.onResult(chromeBitmap);
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, url, mFaviconSize, (image, iconUrl) -> {
                        Bitmap favicon;
                        if (image == null) {
                            favicon = getResizedBitmapFromDrawable(
                                    AppCompatResources.getDrawable(mContext, R.drawable.ic_globe_24dp),
                                    mDefaultFaviconSize);
                        } else {
                            favicon = image;
                        }
                        faviconCallback.onResult(favicon);
                    });
        }
    }

    /**
     * Asynchronously get the processed {@link TabFavicon}.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    public void getFaviconForUrlAsync(
            GURL url, boolean isIncognito, Callback<TabFavicon> faviconCallback) {
        if (mFaviconHelper == null || UrlUtilities.isNTPUrl(url)) {
            faviconCallback.onResult(getRoundedChromeFavicon(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, url, mFaviconSize, (image, iconUrl) -> {
                        TabFavicon favicon;
                        if (image == null) {
                            favicon = getRoundedGlobeFavicon(isIncognito);
                        } else if (UrlUtilities.isInternalScheme(url) && !mIsTabStrip) {
                            Bitmap resizedFavicon = getResizedBitmapFromDrawable(
                                    processBitmap(image, false), mDefaultFaviconSize);
                            favicon = isIncognito ? createChromeOwnedTabFavicon(
                                    resizedFavicon, 0, mIncognitoSelectedIconColor, true)
                                    : createChromeOwnedTabFavicon(resizedFavicon, 0,
                                    mSelectedIconColor, true);
                        } else {
                            favicon = new TabFavicon(processBitmap(image, mIsTabStrip),
                                    ColorExtractor.findDominantColorByHue(image));
                        }
                        faviconCallback.onResult(favicon);
                    });
        }
    }

    /**
     * Synchronously get the processed favicon, assuming it is not recolor allowed.
     * @param icon The favicon that was received.
     * @return The processed {@link TabFavicon}.
     */
    TabFavicon getFaviconFromBitmap(@NonNull Bitmap icon) {
        return new TabFavicon(processBitmap(icon, mIsTabStrip),
                ColorExtractor.findDominantColorByHue(icon));
    }

    /**
     * Asynchronously get the composed, up to 4, {{@link TabFavicon}}.
     * @param urls List of urls, up to 4, whose favicon are requested to be composed.
     * @param isIncognito Whether the processed composed favicon is used for incognito or not.
     * @param faviconCallback The callback that requests for the composed favicon.
     */
    void getComposedFaviconImageAsync(
            List<GURL> urls, boolean isIncognito, Callback<TabFavicon> faviconCallback) {
        assert urls != null && urls.size() > 1 && urls.size() <= 4;

        mFaviconHelper.getComposedFaviconImage(mProfile, urls, mFaviconSize, (image, iconUrl) -> {
            if (image == null) {
                faviconCallback.onResult(getDefaultComposedImageFavicon(isIncognito));
            } else {
                faviconCallback.onResult(new TabFavicon(processBitmap(image, mIsTabStrip),
                        ColorExtractor.findDominantColorByHue(image)));
            }
        });
    }

    private TabFavicon getDefaultComposedImageFavicon(boolean isIncognito) {
        return isIncognito ? sRoundedComposedDefaultFaviconIncognito
                : colorFaviconWithTheme(sRoundedComposedDefaultFavicon);
    }

    private TabFavicon getRoundedChromeFavicon(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedChromeFaviconForStrip;
        }
        return isIncognito ? sRoundedChromeFaviconIncognito
                : colorFaviconWithTheme(sRoundedChromeFavicon);
    }

    private TabFavicon getRoundedGlobeFavicon(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedGlobeFaviconForStrip;
        }
        return isIncognito ? sRoundedGlobeFaviconIncognito
                : colorFaviconWithTheme(sRoundedGlobeFavicon);
    }

    private TabFavicon createChromeOwnedTabFavicon(Bitmap bitmap, @ColorInt int colorDefault,
                                                   @ColorInt int colorSelected, boolean useBitmapColorInDefault) {
        Drawable defaultDrawable = processBitmap(bitmap, false);
        if (!useBitmapColorInDefault) {
            defaultDrawable.setColorFilter(
                    new PorterDuffColorFilter(colorDefault, PorterDuff.Mode.SRC_IN));
        }

        Drawable selectedDrawable = processBitmap(bitmap, false);
        selectedDrawable.setColorFilter(
                new PorterDuffColorFilter(colorSelected, PorterDuff.Mode.SRC_IN));
        return new TabFavicon(defaultDrawable, selectedDrawable, true,
                ColorExtractor.findDominantColorByHue(bitmap));
    }

    /**
     * Update the favicon color used in normal mode (non-incognito) with latest color setting.
     * Return the same {@link TabFavicon} with updated color in its drawable(s).
     *
     * TODO(https://crbug.com/1234953): Avoid creating color filter every time.
     */
    private TabFavicon colorFaviconWithTheme(TabFavicon favicon) {
        assert favicon.isRecolorAllowed();

        int colorDefault = getTitleTextColor(mContext, false, false);
        favicon.getDefaultDrawable().setColorFilter(
                new PorterDuffColorFilter(colorDefault, PorterDuff.Mode.SRC_IN));

        if (favicon.hasSelectedState()) {
            int colorSelected = getTitleTextColor(mContext, false, true);
            favicon.getSelectedDrawable().setColorFilter(
                    new PorterDuffColorFilter(colorSelected, PorterDuff.Mode.SRC_IN));
        }

        return favicon;
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The text appearance for the tab grid card title.
     */
    public static @ColorInt int getTitleTextColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            @ColorRes
            int colorRes = isSelected ? R.color.incognito_tab_title_selected_color
                    : R.color.incognito_tab_title_color;
            return context.getResources().getColor(colorRes);
        } else {

            if (isSelected) {
                return SkinEngine.getColor(context, R.attr.colorPrimary);
            }

            return Color.BLACK;
        }
    }
}

