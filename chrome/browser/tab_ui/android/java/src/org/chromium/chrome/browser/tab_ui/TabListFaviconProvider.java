// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

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
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
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

/** Provider for processed favicons in Tab list. */
public class TabListFaviconProvider {
    /**
     * Interface for lazily fetching favicons. Instances of this class should implement the fetch
     * method to resolve to an appropriate favicon returned via callback when invoked.
     */
    public interface TabFaviconFetcher {
        /**
         * Asynchronously fetches a tab favicon.
         *
         * @param faviconCallback Called once with a favicon for the tab. Payload may be null.
         */
        void fetch(Callback<TabFavicon> faviconCallback);
    }

    /**
     * Wrapper class that holds a favicon drawable and whether recolor is allowed. Subclasses should
     * make a best effort to implement an {@link Object#equals(Object)} that will allow efficient
     * comparisons of favicon objects.
     */
    public abstract static class TabFavicon {
        private final @NonNull Drawable mDefaultDrawable;
        private final @NonNull Drawable mSelectedDrawable;
        private final boolean mIsRecolorAllowed;

        protected TabFavicon(
                @NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable,
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
    public static class UrlTabFavicon extends TabFavicon {
        private final @NonNull GURL mGurl;

        private UrlTabFavicon(
                @NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable,
                boolean allowRecolor,
                @NonNull GURL gurl) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mGurl = gurl;
        }

        @VisibleForTesting
        public UrlTabFavicon(@NonNull Drawable drawable, @NonNull GURL gurl) {
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
    public static class ComposedTabFavicon extends TabFavicon {
        private final @NonNull GURL[] mGurls;

        @VisibleForTesting
        public ComposedTabFavicon(@NonNull Drawable drawable, @NonNull GURL[] gurls) {
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
    public @interface StaticTabFaviconType {
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
    public static class ResourceTabFavicon extends TabFavicon {
        private final @StaticTabFaviconType int mType;

        private ResourceTabFavicon(
                @NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable,
                boolean allowRecolor,
                @StaticTabFaviconType int type) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mType = type;
        }

        @VisibleForTesting
        public ResourceTabFavicon(@NonNull Drawable defaultDrawable, @StaticTabFaviconType int type) {
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

    private interface TabFaviconResolver {
        /** Attempts to create a {@link TabFavicon} from {@link Context}. */
        @Nullable
        TabFavicon resolve(Context context);
    }

    /** Lazily resolves a static {@link TabFavicon}. */
    private static class LazyTabFaviconResolver {
        // Null after resolution succeeds.
        private @Nullable TabFaviconResolver mResolver;
        // Null until resolution succeeds.
        private @Nullable TabFavicon mTabFavicon;

        LazyTabFaviconResolver(TabFaviconResolver resolver) {
            assert resolver != null;
            mResolver = resolver;
        }

        TabFavicon get(Context context) {
            if (mTabFavicon == null) {
                mTabFavicon = mResolver.resolve(context);
                if (mTabFavicon != null) {
                    mResolver = null;
                }
            }
            return mTabFavicon;
        }
    }

    private static LazyTabFaviconResolver sRoundedGlobeFavicon;
    private static LazyTabFaviconResolver sRoundedGlobeFaviconForStrip;
    private static LazyTabFaviconResolver sRoundedGlobeFaviconIncognito;
    private static LazyTabFaviconResolver sRoundedComposedDefaultFavicon;
    private static LazyTabFaviconResolver sRoundedComposedDefaultFaviconIncognito;

    /** These icons may fail to load. See crbug.com/324996488. */
    private static LazyTabFaviconResolver sRoundedChromeFavicon;

    private static LazyTabFaviconResolver sRoundedChromeFaviconIncognito;
    private static LazyTabFaviconResolver sRoundedChromeFaviconForStrip;

    private final @ColorInt int mSelectedIconColor;
    private final @ColorInt int mIncognitoSelectedIconColor;
    private final int mStripFaviconSize;
    private final int mDefaultFaviconSize;
    private final int mFaviconSize;
    private final int mFaviconInset;
    private final int mFaviconCornerRadius;
    private final Context mContext;
    private final boolean mIsTabStrip;

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
        mFaviconInset =
                ViewUtils.dpToPx(
                        context,
                        context.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_favicon_inset));
        mIsTabStrip = isTabStrip;
        mFaviconCornerRadius = context.getResources().getDimensionPixelSize(faviconCornerRadiusId);

        @ColorInt
        int defaultIconColor =
                TabUiThemeUtils.getChromeOwnedFaviconTintColor(context, false, false);
        mSelectedIconColor = TabUiThemeUtils.getChromeOwnedFaviconTintColor(context, false, true);
        @ColorInt
        int incognitoIconColor =
                TabUiThemeUtils.getChromeOwnedFaviconTintColor(context, true, false);
        mIncognitoSelectedIconColor =
                TabUiThemeUtils.getChromeOwnedFaviconTintColor(context, true, true);
        maybeSetUpLazyTabFaviconResolvers(
                defaultIconColor,
                mSelectedIconColor,
                incognitoIconColor,
                mIncognitoSelectedIconColor,
                mDefaultFaviconSize,
                mStripFaviconSize,
                mFaviconCornerRadius,
                mFaviconInset);
    }

    /**
     * Initializes with native.
     *
     * @param profile The profile to use for the favicon database.
     */
    public void initWithNative(Profile profile) {
        if (mIsInitialized) return;

        mProfile = profile;
        assert mProfile != null : "Profile must exist for favicon fetching.";
        mFaviconHelper = new FaviconHelper();
        mIsInitialized = true;
    }

    /** Returns whether native has been initialized. */
    public boolean isInitialized() {
        return mIsInitialized;
    }

    /** Override the favicon helper for unit tests. */
    void setFaviconHelperForTesting(FaviconHelper faviconHelper) {
        mFaviconHelper = faviconHelper;
    }

    /**
     * Create a fetcher for the scaled rounded globe drawable used for default favicon. Used when
     * favicon is static and not changing colors when its parent component is selected.
     *
     * @param isIncognito Whether the {@link TabFavicon} is used for incognito mode.
     * @return a fetcher for the scaled rounded globe drawable used for default favicon.
     */
    public TabFaviconFetcher getDefaultFaviconFetcher(boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                faviconCallback.onResult(getDefaultFavicon(isIncognito));
            }
        };
    }

    /**
     * Creates a fetcher that asynchronously fetches a favicon. Used when favicon is static and not
     * changing colors when its parent component is selected.
     *
     * @param url The URL to get a favicon for
     * @param isIncognito Whether the style is for incognito.
     * @return a favicon fetcher to fetch the favicon from native.
     */
    public TabFaviconFetcher getFaviconForUrlFetcher(GURL url, boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                getFaviconForUrlAsync(url, isIncognito, faviconCallback);
            }
        };
    }

    /**
     * Asynchronously get the processed favicon as a {@link Drawable}.
     *
     * @param url The URL to get a favicon for
     * @param isIncognito Whether the style is for incognito.
     * @param faviconCallback The callback to be serviced with the drawable when ready.
     */
    public void getFaviconDrawableForUrlAsync(
            GURL url, boolean isIncognito, Callback<Drawable> faviconCallback) {
        getFaviconForUrlAsync(
                url,
                isIncognito,
                tabFavicon -> faviconCallback.onResult(tabFavicon.getDefaultDrawable()));
    }

    /**
     * Create a fetcher that synchronously gets the processed favicon using the provided bitmap,
     * assuming it is not recolor allowed.
     *
     * @param icon The favicon that was received.
     * @param iconUrl The url the favicon came from.
     * @return a favicon fetcher that returns a processed version of the bitmap.
     */
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
     * Creates a fetcher that asynchronously creates a composed, up to 4 favicon, {{@link
     * TabFavicon}}.
     *
     * @param urls List of urls, up to 4, whose favicon are requested to be composed.
     * @param isIncognito Whether the processed composed favicon is used for incognito or not.
     * @return a favicon fetcher that returns the composed favicon.
     */
    public TabFaviconFetcher getComposedFaviconImageFetcher(List<GURL> urls, boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                getComposedFaviconImageAsync(urls, isIncognito, faviconCallback);
            }
        };
    }

    /** Returns the rounded Chrome favicon to use for native or internal pages. */
    public TabFavicon getRoundedChromeFavicon(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedChromeFaviconForStrip.get(mContext);
        }
        // Fallback if the bitmap decoding failed.
        if (isIncognito
                ? (sRoundedChromeFaviconIncognito.get(mContext) == null)
                : (sRoundedChromeFavicon.get(mContext) == null)) {
            return getRoundedGlobeFavicon(isIncognito);
        }
        return isIncognito
                ? sRoundedChromeFaviconIncognito.get(mContext)
                : colorFaviconWithTheme(sRoundedChromeFavicon.get(mContext));
    }

    /** Returns the default globe favicon. Visible for testing to override return value. */
    @VisibleForTesting
    public TabFavicon getDefaultFavicon(boolean isIncognito) {
        return getRoundedGlobeFavicon(isIncognito);
    }

    /** Returns the bitmap as a favicon. Visible for testing to override return value. */
    @VisibleForTesting
    public TabFavicon getFaviconFromBitmap(@NonNull Bitmap icon, @NonNull GURL iconUrl) {
        return new UrlTabFavicon(processBitmap(icon, mIsTabStrip), iconUrl);
    }

    /**
     * Asynchronously creates a {@link TabFavicon} for a URL. Visible for testing to override return
     * value. Prefer {@link #getFaviconDrawableForUrlAsync} or {@link #getFaviconForUrlFetcher}.
     *
     * @param url The URL of the tab whose favicon is being requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    @VisibleForTesting
    public void getFaviconForUrlAsync(
            GURL url, boolean isIncognito, Callback<TabFavicon> faviconCallback) {
        if (mFaviconHelper == null || UrlUtilities.isNtpUrl(url)) {
            faviconCallback.onResult(getRoundedChromeFavicon(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    getProfile(isIncognito),
                    url,
                    mFaviconSize,
                    (image, iconUrl) -> {
                        TabFavicon favicon;
                        if (image == null) {
                            favicon = getRoundedGlobeFavicon(isIncognito);
                        } else if (UrlUtilities.isInternalScheme(url) && !mIsTabStrip) {
                            Bitmap resizedFavicon =
                                    getResizedBitmapFromDrawable(
                                            processBitmap(image, false), mDefaultFaviconSize);
                            favicon =
                                    isIncognito
                                            ? createChromeOwnedUrlTabFavicon(
                                                    resizedFavicon,
                                                    0,
                                                    mIncognitoSelectedIconColor,
                                                    true,
                                                    iconUrl)
                                            : createChromeOwnedUrlTabFavicon(
                                                    resizedFavicon,
                                                    0,
                                                    mSelectedIconColor,
                                                    true,
                                                    iconUrl);
                        } else {
                            favicon = new UrlTabFavicon(processBitmap(image, mIsTabStrip), iconUrl);
                        }
                        faviconCallback.onResult(favicon);
                    });
        }
    }

    private void getComposedFaviconImageAsync(
            List<GURL> urls, boolean isIncognito, Callback<TabFavicon> faviconCallback) {
        assert urls != null && urls.size() > 1 && urls.size() <= 4;
        mFaviconHelper.getComposedFaviconImage(
                getProfile(isIncognito),
                urls,
                mFaviconSize,
                (image, iconUrls) -> {
                    if (image == null) {
                        faviconCallback.onResult(getDefaultComposedImageFavicon(isIncognito));
                    } else {
                        faviconCallback.onResult(
                                new ComposedTabFavicon(
                                        processBitmap(image, mIsTabStrip), iconUrls));
                    }
                });
    }

    private TabFavicon getDefaultComposedImageFavicon(boolean isIncognito) {
        return isIncognito
                ? sRoundedComposedDefaultFaviconIncognito.get(mContext)
                : colorFaviconWithTheme(sRoundedComposedDefaultFavicon.get(mContext));
    }

    private TabFavicon getRoundedGlobeFavicon(boolean isIncognito) {
        if (mIsTabStrip) {
            return sRoundedGlobeFaviconForStrip.get(mContext);
        }
        return isIncognito
                ? sRoundedGlobeFaviconIncognito.get(mContext)
                : colorFaviconWithTheme(sRoundedGlobeFavicon.get(mContext));
    }

    private TabFavicon createChromeOwnedUrlTabFavicon(
            Bitmap bitmap,
            @ColorInt int colorDefault,
            @ColorInt int colorSelected,
            boolean useBitmapColorInDefault,
            GURL gurl) {
        Drawable defaultDrawable =
                processBitmapMaybeColor(
                        mContext,
                        bitmap,
                        mDefaultFaviconSize,
                        mFaviconCornerRadius,
                        !useBitmapColorInDefault,
                        colorDefault);
        Drawable selectedDrawable =
                processBitmapMaybeColor(
                        mContext,
                        bitmap,
                        mDefaultFaviconSize,
                        mFaviconCornerRadius,
                        true,
                        colorSelected);
        return new UrlTabFavicon(defaultDrawable, selectedDrawable, true, gurl);
    }

    private static TabFavicon createChromeOwnedResourceTabFavicon(
            Context context,
            Bitmap bitmap,
            int size,
            int cornerRadius,
            @ColorInt int colorDefault,
            @ColorInt int colorSelected,
            boolean useBitmapColorInDefault,
            @StaticTabFaviconType int type) {
        Drawable defaultDrawable =
                processBitmapMaybeColor(
                        context,
                        bitmap,
                        size,
                        cornerRadius,
                        !useBitmapColorInDefault,
                        colorDefault);
        Drawable selectedDrawable =
                processBitmapMaybeColor(context, bitmap, size, cornerRadius, true, colorSelected);
        return new ResourceTabFavicon(defaultDrawable, selectedDrawable, true, type);
    }

    private static Drawable processBitmapMaybeColor(
            Context context,
            Bitmap bitmap,
            int size,
            int cornerRadius,
            boolean shouldSetColor,
            @ColorInt int color) {
        Drawable drawable = processBitmapNoBackground(context, bitmap, size, cornerRadius);
        if (shouldSetColor) {
            drawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
        }
        return drawable;
    }

    /**
     * Update the favicon color used in normal mode (non-incognito) with latest color setting.
     * Return the same {@link TabFavicon} with updated color in its drawable(s).
     *
     * <p>TODO(crbug.com/40781763): Avoid creating color filter every time.
     */
    private TabFavicon colorFaviconWithTheme(TabFavicon favicon) {
        assert favicon.isRecolorAllowed();

        int colorDefault = TabUiThemeUtils.getChromeOwnedFaviconTintColor(mContext, false, false);
        favicon.getDefaultDrawable()
                .setColorFilter(new PorterDuffColorFilter(colorDefault, PorterDuff.Mode.SRC_IN));

        if (favicon.hasSelectedState()) {
            int colorSelected =
                    TabUiThemeUtils.getChromeOwnedFaviconTintColor(mContext, false, true);
            favicon.getSelectedDrawable()
                    .setColorFilter(
                            new PorterDuffColorFilter(colorSelected, PorterDuff.Mode.SRC_IN));
        }

        return favicon;
    }

    private Profile getProfile(boolean isIncognito) {
        if (!isIncognito) return mProfile;

        Profile otrProfile = mProfile.getPrimaryOTRProfile(/* createIfNeeded= */ false);
        assert otrProfile != null : "Requesting favicon for OTR Profile when none exists.";
        return otrProfile;
    }

    private static Bitmap getResizedBitmapFromDrawable(Drawable drawable, int size) {
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, size, size);
        drawable.draw(canvas);
        return bitmap;
    }

    private Drawable processBitmap(Bitmap bitmap, boolean isTabStrip) {
        if (isTabStrip) {
            return processBitampWithBackground(
                    mContext, bitmap, mStripFaviconSize, mFaviconCornerRadius, mFaviconInset);
        } else {
            return processBitmapNoBackground(
                    mContext, bitmap, mDefaultFaviconSize, mFaviconCornerRadius);
        }
    }

    private static Drawable processBitmapNoBackground(
            Context context, Bitmap bitmap, int size, int cornerRadius) {
        return ViewUtils.createRoundedBitmapDrawable(
                context.getResources(),
                Bitmap.createScaledBitmap(bitmap, size, size, true),
                cornerRadius);
    }

    private static Drawable processBitampWithBackground(
            Context context, Bitmap bitmap, int size, int cornerRadius, int inset) {
        Drawable favicon = processBitmapNoBackground(context, bitmap, size, cornerRadius);
        Drawable circleBackground =
                AppCompatResources.getDrawable(context, R.drawable.tab_strip_favicon_circle);
        Drawable[] layers = {circleBackground, favicon};
        LayerDrawable layerDrawable = new LayerDrawable(layers);
        layerDrawable.setLayerInset(1, inset, inset, inset, inset);
        return layerDrawable;
    }

    private static void maybeSetUpLazyTabFaviconResolvers(
            @ColorInt int defaultIconColor,
            @ColorInt int selectedIconColor,
            @ColorInt int incognitoIconColor,
            @ColorInt int incognitoSelectedIconColor,
            int defaultFaviconSize,
            int stripFaviconSize,
            int cornerRadius,
            int inset) {
        if (sRoundedGlobeFavicon == null) {
            sRoundedGlobeFavicon =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                // TODO(crbug.com/40682607): From Android Developer Documentation,
                                // we should avoid resizing vector drawables.
                                Bitmap globeBitmap =
                                        getResizedBitmapFromDrawable(
                                                AppCompatResources.getDrawable(
                                                        context, R.drawable.ic_globe_24dp),
                                                defaultFaviconSize);
                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        globeBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        defaultIconColor,
                                        selectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_GLOBE);
                            });
        }
        if (sRoundedChromeFavicon == null) {
            sRoundedChromeFavicon =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Bitmap chromeBitmap =
                                        BitmapFactory.decodeResource(
                                                context.getResources(), R.drawable.chromelogo16);
                                if (chromeBitmap == null) return null;

                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        chromeBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        defaultIconColor,
                                        selectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_CHROME);
                            });
        }
        if (sRoundedComposedDefaultFavicon == null) {
            sRoundedComposedDefaultFavicon =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Bitmap composedBitmap =
                                        getResizedBitmapFromDrawable(
                                                AppCompatResources.getDrawable(
                                                        context, R.drawable.ic_group_icon_16dp),
                                                defaultFaviconSize);
                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        composedBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        defaultIconColor,
                                        selectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT);
                            });
        }
        if (sRoundedGlobeFaviconIncognito == null) {
            sRoundedGlobeFaviconIncognito =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Bitmap globeBitmap =
                                        getResizedBitmapFromDrawable(
                                                AppCompatResources.getDrawable(
                                                        context, R.drawable.ic_globe_24dp),
                                                defaultFaviconSize);
                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        globeBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        incognitoIconColor,
                                        incognitoSelectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_GLOBE_INCOGNITO);
                            });
        }
        if (sRoundedChromeFaviconIncognito == null) {
            sRoundedChromeFaviconIncognito =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Bitmap chromeBitmap =
                                        BitmapFactory.decodeResource(
                                                context.getResources(), R.drawable.chromelogo16);
                                if (chromeBitmap == null) return null;

                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        chromeBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        incognitoIconColor,
                                        incognitoSelectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_CHROME_INCOGNITO);
                            });
        }
        if (sRoundedComposedDefaultFaviconIncognito == null) {
            sRoundedComposedDefaultFaviconIncognito =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Bitmap composedBitmap =
                                        getResizedBitmapFromDrawable(
                                                AppCompatResources.getDrawable(
                                                        context, R.drawable.ic_group_icon_16dp),
                                                defaultFaviconSize);
                                return createChromeOwnedResourceTabFavicon(
                                        context,
                                        composedBitmap,
                                        defaultFaviconSize,
                                        cornerRadius,
                                        incognitoIconColor,
                                        incognitoSelectedIconColor,
                                        false,
                                        StaticTabFaviconType.ROUNDED_COMPOSED_DEFAULT_INCOGNITO);
                            });
        }

        // Tab strip favicons do not recolor when selected.
        if (sRoundedGlobeFaviconForStrip == null) {
            sRoundedGlobeFaviconForStrip =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Drawable globeDrawable =
                                        AppCompatResources.getDrawable(
                                                context, R.drawable.ic_globe_24dp);
                                return new ResourceTabFavicon(
                                        processBitampWithBackground(
                                                context,
                                                getResizedBitmapFromDrawable(
                                                        globeDrawable, stripFaviconSize),
                                                stripFaviconSize,
                                                cornerRadius,
                                                inset),
                                        StaticTabFaviconType.ROUNDED_GLOBE_FOR_STRIP);
                            });
        }
        if (sRoundedChromeFaviconForStrip == null) {
            sRoundedChromeFaviconForStrip =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Drawable chromeDrawable =
                                        AppCompatResources.getDrawable(
                                                context, R.drawable.chromelogo16);
                                return new ResourceTabFavicon(
                                        processBitampWithBackground(
                                                context,
                                                getResizedBitmapFromDrawable(
                                                        chromeDrawable, stripFaviconSize),
                                                stripFaviconSize,
                                                cornerRadius,
                                                inset),
                                        StaticTabFaviconType.ROUNDED_CHROME_FOR_STRIP);
                            });
        }
    }
}
