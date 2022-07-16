// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {
    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;

    /**
     * Wrapper class that holds a favicon drawable and whether recolor is allowed.
     */
    static class TabFavicon {
        private final @NonNull Drawable mDefaultDrawable;
        private final @NonNull Drawable mSelectedDrawable;
        private final boolean mIsRecolorAllowed;

        private TabFavicon(@NonNull Drawable defaultDrawable) {
            this(defaultDrawable, defaultDrawable, false);
        }

        private TabFavicon(@NonNull Drawable defaultDrawable, @NonNull Drawable selectedDrawable,
                boolean allowRecolor) {
            mDefaultDrawable = defaultDrawable;
            mSelectedDrawable = selectedDrawable;
            mIsRecolorAllowed = allowRecolor;
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

        mDefaultIconColor =
                TabUiThemeProvider.getChromeOwnedFaviconTintColor(context, false, false);
        mSelectedIconColor =
                TabUiThemeProvider.getChromeOwnedFaviconTintColor(context, false, true);
        mIncognitoIconColor =
                TabUiThemeProvider.getChromeOwnedFaviconTintColor(context, true, false);
        mIncognitoSelectedIconColor =
                TabUiThemeProvider.getChromeOwnedFaviconTintColor(context, true, true);

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
            sRoundedGlobeFaviconForStrip = new TabFavicon(processBitmap(
                    getResizedBitmapFromDrawable(globeDrawable, mStripFaviconSize), true));
        }
        if (sRoundedChromeFaviconForStrip == null) {
            Drawable chromeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.chromelogo16);
            sRoundedChromeFaviconForStrip = new TabFavicon(processBitmap(
                    getResizedBitmapFromDrawable(chromeDrawable, mStripFaviconSize), true));
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

    /**
     * Asynchronously get the processed {@link TabFavicon}.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    void getFaviconForUrlAsync(
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
                            favicon = new TabFavicon(processBitmap(image, mIsTabStrip));
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
        return new TabFavicon(processBitmap(icon, mIsTabStrip));
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
                faviconCallback.onResult(new TabFavicon(processBitmap(image, mIsTabStrip)));
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
        return new TabFavicon(defaultDrawable, selectedDrawable, true);
    }

    /**
     * Update the favicon color used in normal mode (non-incognito) with latest color setting.
     * Return the same {@link TabFavicon} with updated color in its drawable(s).
     *
     * TODO(https://crbug.com/1234953): Avoid creating color filter every time.
     */
    private TabFavicon colorFaviconWithTheme(TabFavicon favicon) {
        assert favicon.isRecolorAllowed();

        int colorDefault =
                TabUiThemeProvider.getChromeOwnedFaviconTintColor(mContext, false, false);
        favicon.getDefaultDrawable().setColorFilter(
                new PorterDuffColorFilter(colorDefault, PorterDuff.Mode.SRC_IN));

        if (favicon.hasSelectedState()) {
            int colorSelected =
                    TabUiThemeProvider.getChromeOwnedFaviconTintColor(mContext, false, true);
            favicon.getSelectedDrawable().setColorFilter(
                    new PorterDuffColorFilter(colorSelected, PorterDuff.Mode.SRC_IN));
        }

        return favicon;
    }
}
