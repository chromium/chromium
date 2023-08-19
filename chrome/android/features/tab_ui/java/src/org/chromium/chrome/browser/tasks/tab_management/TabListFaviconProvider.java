// Copyright 2019 The Chromium Authors
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
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {
    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;

    /**
     * Wrapper class that holds a favicon drawable and whether recolor is allowed. Subclasses should
     * make a best effort to implement an {@link Object#equals(Object)} that will allow efficient
     * comparisons of favicon objects.
     */
    abstract static class TabFavicon {
        private final @NonNull Drawable mDefaultDrawable;
        private final @NonNull Drawable mSelectedDrawable;
        private final boolean mIsRecolorAllowed;

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

        @Override
        public abstract boolean equals(Object other);
    }

    /** A favicon that is sourced from and equality checked on a single URL. */
    @VisibleForTesting
    static class UrlTabFavicon extends TabFavicon {
        private final @NonNull GURL mGurl;

        private UrlTabFavicon(@NonNull Drawable defaultDrawable, @NonNull Drawable selectedDrawable,
                boolean allowRecolor, @NonNull GURL gurl) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mGurl = gurl;
        }

        @VisibleForTesting
        UrlTabFavicon(@NonNull Drawable drawable, @NonNull GURL gurl) {
            this(drawable, drawable, false, gurl);
        }

        @Override
        public int hashCode() {
            return mGurl.hashCode();
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof UrlTabFavicon)) {
                return false;
            }
            return Objects.equals(mGurl, ((UrlTabFavicon) other).mGurl);
        }
    }

    /** Tracks the GURLS that were used for the composed favicon for the equality check.  */
    @VisibleForTesting
    static class ComposedTabFavicon extends TabFavicon {
        private final @NonNull GURL[] mGurls;

        @VisibleForTesting
        ComposedTabFavicon(@NonNull Drawable drawable, @NonNull GURL[] gurls) {
            super(drawable, drawable, false);
            mGurls = gurls;
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(mGurls);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof ComposedTabFavicon)) {
                return false;
            }
            return Arrays.equals(mGurls, ((ComposedTabFavicon) other).mGurls);
        }
    }

    @IntDef({
            StaticTabFaviconType.UNKNOWN,
            StaticTabFaviconType.ROUNDED_GLOBE,
            StaticTabFaviconType.ROUNDED_CHROME,
            StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT,
            StaticTabFaviconType.ROUNDED_GLOBE_INCOGNITO,
            StaticTabFaviconType.ROUNDED_CHROME_INCOGNITO,
            StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT_INCOGNITO,
            StaticTabFaviconType.ROUNDED_GLOBE_FOR_STRIP,
            StaticTabFaviconType.ROUNDED_CHROME_FOR_STRIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface StaticTabFaviconType {
        int UNKNOWN = 0;
        int ROUNDED_GLOBE = 1;
        int ROUNDED_CHROME = 2;
        int ROUNDED_COMPOSED_DEFAULT = 3;
        int ROUNDED_GLOBE_INCOGNITO = 4;
        int ROUNDED_CHROME_INCOGNITO = 5;
        int ROUNDED_COMPOSED_DEFAULT_INCOGNITO = 6;
        int ROUNDED_GLOBE_FOR_STRIP = 7;
        int ROUNDED_CHROME_FOR_STRIP = 8;
    }

    /** A favicon that is one of a fixed number of static icons. */
    @VisibleForTesting
    static class ResourceTabFavicon extends TabFavicon {
        private final @StaticTabFaviconType int mType;

        private ResourceTabFavicon(@NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable, boolean allowRecolor,
                @StaticTabFaviconType int type) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mType = type;
        }

        @VisibleForTesting
        ResourceTabFavicon(@NonNull Drawable defaultDrawable, @StaticTabFaviconType int type) {
            this(defaultDrawable, defaultDrawable, false, type);
        }

        @Override
        public int hashCode() {
            return Integer.hashCode(mType);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof ResourceTabFavicon)) {
                return false;
            }
            return this.mType == ((ResourceTabFavicon) other).mType;
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
    private final int mFaviconCornerRadius;
    private boolean mIsInitialized;

    private Profile mProfile;
    private FaviconHelper mFaviconHelper;

    /**
     * Construct the provider that provides favicons for tab list.
     * @param context    The context to use for accessing {@link android.content.res.Resources}
     * @param isTabStrip Indicator for whether this class provides favicons for tab strip or not.
     * @param faviconCornerRadiusId The resource Id for the favicon corner radius.
     *
     */
    public TabListFaviconProvider(Context context, boolean isTabStrip, int faviconCornerRadiusId) {
        mContext = context;
        mDefaultFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        mStripFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_size);
        mFaviconSize = isTabStrip ? mStripFaviconSize : mDefaultFaviconSize;
        mFaviconInset = ViewUtils.dpToPx(context,
                context.getResources().getDimensionPixelSize(R.dimen.tab_strip_favicon_inset));
        mIsTabStrip = isTabStrip;
        mFaviconCornerRadius = context.getResources().getDimensionPixelSize(faviconCornerRadiusId);

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
            sRoundedGlobeFavicon =
                    createChromeOwnedResourceTabFavicon(globeBitmap, mDefaultIconColor,
                            mSelectedIconColor, false, StaticTabFaviconType.ROUNDED_GLOBE);
        }
        if (sRoundedChromeFavicon == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeFavicon =
                    createChromeOwnedResourceTabFavicon(chromeBitmap, mDefaultIconColor,
                            mSelectedIconColor, false, StaticTabFaviconType.ROUNDED_CHROME);
        }
        if (sRoundedComposedDefaultFavicon == null) {
            Bitmap composedBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp),
                    mDefaultFaviconSize);
            sRoundedComposedDefaultFavicon = createChromeOwnedResourceTabFavicon(composedBitmap,
                    mDefaultIconColor, mSelectedIconColor, false,
                    StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT);
        }
        if (sRoundedGlobeFaviconIncognito == null) {
            Bitmap globeBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp),
                    mDefaultFaviconSize);
            sRoundedGlobeFaviconIncognito = createChromeOwnedResourceTabFavicon(globeBitmap,
                    mIncognitoIconColor, mIncognitoSelectedIconColor, false,
                    StaticTabFaviconType.ROUNDED_GLOBE_INCOGNITO);
        }
        if (sRoundedChromeFaviconIncognito == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeFaviconIncognito = createChromeOwnedResourceTabFavicon(chromeBitmap,
                    mIncognitoIconColor, mIncognitoSelectedIconColor, false,
                    StaticTabFaviconType.ROUNDED_CHROME_INCOGNITO);
        }
        if (sRoundedComposedDefaultFaviconIncognito == null) {
            Bitmap composedBitmap = getResizedBitmapFromDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp),
                    mDefaultFaviconSize);
            sRoundedComposedDefaultFaviconIncognito = createChromeOwnedResourceTabFavicon(
                    composedBitmap, mIncognitoIconColor, mIncognitoSelectedIconColor, false,
                    StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT_INCOGNITO);
        }

        // Tab strip favicons do not recolor when selected.
        if (sRoundedGlobeFaviconForStrip == null) {
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            sRoundedGlobeFaviconForStrip = new ResourceTabFavicon(
                    processBitmap(
                            getResizedBitmapFromDrawable(globeDrawable, mStripFaviconSize), true),
                    StaticTabFaviconType.ROUNDED_GLOBE_FOR_STRIP);
        }
        if (sRoundedChromeFaviconForStrip == null) {
            Drawable chromeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.chromelogo16);
            sRoundedChromeFaviconForStrip = new ResourceTabFavicon(
                    processBitmap(
                            getResizedBitmapFromDrawable(chromeDrawable, mStripFaviconSize), true),
                    StaticTabFaviconType.ROUNDED_CHROME_FOR_STRIP);
        }
    }

    public void initForTesting(Profile profile, FaviconHelper helper) {
        assert !mIsInitialized;
        mProfile = profile;
        mFaviconHelper = helper;
        mIsInitialized = true;
    }

    public void initWithNative(Profile profile) {
        if (mIsInitialized) return;

        mProfile = profile;
        assert mProfile != null : "Profile must exist for favicon fetching.";
        mFaviconHelper = new FaviconHelper();
        mIsInitialized = true;
    }

    private Profile getProfile(boolean isIncognito) {
        if (!isIncognito) return mProfile;

        Profile otrProfile = mProfile.getPrimaryOTRProfile(/*createIfNeeded=*/false);
        assert otrProfile != null : "Requesting favicon for OTR Profile when none exists.";
        return otrProfile;
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
        Drawable favicon = ViewUtils.createRoundedBitmapDrawable(mContext.getResources(),
                Bitmap.createScaledBitmap(bitmap, size, size, true), mFaviconCornerRadius);
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
     * Interface for lazily fetching favicons. Instances of this class should implement the fetch
     * method to resolve to an appropriate favicon returned via callback when invoked.
     */
    public interface TabFaviconFetcher {
        /**
         * Asynchronously fetches a tab favicon.
         * @param faviconCallback Called once with a favicon for the tab. Payload may be null.
         */
        public void fetch(Callback<TabFavicon> faviconCallback);
    }

    public TabFaviconFetcher getDefaultFaviconFetcher(boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                faviconCallback.onResult(getDefaultFavicon(isIncognito));
            }
        };
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

    public TabFaviconFetcher getFaviconForUrlFetcher(GURL url, boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                getFaviconForUrlAsync(url, isIncognito, faviconCallback);
            }
        };
    }

    /**
     * Asynchronously get the processed {@link TabFavicon}.
     * @param url The URL of the tab whose favicon is being requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    void getFaviconForUrlAsync(
            GURL url, boolean isIncognito, Callback<TabFavicon> faviconCallback) {
        if (mFaviconHelper == null || UrlUtilities.isNTPUrl(url)) {
            faviconCallback.onResult(getRoundedChromeFavicon(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    getProfile(isIncognito), url, mFaviconSize, (image, iconUrl) -> {
                        TabFavicon favicon;
                        if (image == null) {
                            favicon = getRoundedGlobeFavicon(isIncognito);
                        } else if (UrlUtilities.isInternalScheme(url) && !mIsTabStrip) {
                            Bitmap resizedFavicon = getResizedBitmapFromDrawable(
                                    processBitmap(image, false), mDefaultFaviconSize);
                            favicon = isIncognito ? createChromeOwnedUrlTabFavicon(resizedFavicon,
                                              0, mIncognitoSelectedIconColor, true, iconUrl)
                                                  : createChromeOwnedUrlTabFavicon(resizedFavicon,
                                                          0, mSelectedIconColor, true, iconUrl);
                        } else {
                            favicon = new UrlTabFavicon(processBitmap(image, mIsTabStrip), iconUrl);
                        }
                        faviconCallback.onResult(favicon);
                    });
        }
    }

    public TabFaviconFetcher getFaviconFromBitmapFetcher(
            @NonNull Bitmap icon, @NonNull GURL iconUrl) {
        Drawable processedBitmap = processBitmap(icon, mIsTabStrip);
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                faviconCallback.onResult(new UrlTabFavicon(processedBitmap, iconUrl));
            }
        };
    }

    /**
     * Synchronously get the processed favicon, assuming it is not recolor allowed.
     * @param icon The favicon that was received.
     * @param iconUrl The url the favicon came from.
     * @return The processed {@link TabFavicon}.
     */
    TabFavicon getFaviconFromBitmap(@NonNull Bitmap icon, @NonNull GURL iconUrl) {
        return new UrlTabFavicon(processBitmap(icon, mIsTabStrip), iconUrl);
    }

    public TabFaviconFetcher getComposedFaviconImageFetcher(List<GURL> urls, boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                getComposedFaviconImageAsync(urls, isIncognito, faviconCallback);
            }
        };
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
        mFaviconHelper.getComposedFaviconImage(
                getProfile(isIncognito), urls, mFaviconSize, (image, iconUrls) -> {
                    if (image == null) {
                        faviconCallback.onResult(getDefaultComposedImageFavicon(isIncognito));
                    } else {
                        faviconCallback.onResult(new ComposedTabFavicon(
                                processBitmap(image, mIsTabStrip), iconUrls));
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

    private TabFavicon createChromeOwnedUrlTabFavicon(Bitmap bitmap, @ColorInt int colorDefault,
            @ColorInt int colorSelected, boolean useBitmapColorInDefault, GURL gurl) {
        Drawable defaultDrawable =
                processBitmapMaybeColor(bitmap, !useBitmapColorInDefault, colorDefault);
        Drawable selectedDrawable = processBitmapMaybeColor(bitmap, true, colorSelected);
        return new UrlTabFavicon(defaultDrawable, selectedDrawable, true, gurl);
    }

    private TabFavicon createChromeOwnedResourceTabFavicon(Bitmap bitmap,
            @ColorInt int colorDefault, @ColorInt int colorSelected,
            boolean useBitmapColorInDefault, @StaticTabFaviconType int type) {
        Drawable defaultDrawable =
                processBitmapMaybeColor(bitmap, !useBitmapColorInDefault, colorDefault);
        Drawable selectedDrawable = processBitmapMaybeColor(bitmap, true, colorSelected);
        return new ResourceTabFavicon(defaultDrawable, selectedDrawable, true, type);
    }

    private Drawable processBitmapMaybeColor(
            Bitmap bitmap, boolean shouldSetColor, @ColorInt int color) {
        Drawable drawable = processBitmap(bitmap, false);
        if (shouldSetColor) {
            drawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
        }
        return drawable;
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
