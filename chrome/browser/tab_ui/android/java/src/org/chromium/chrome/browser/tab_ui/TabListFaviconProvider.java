// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assertNonNull;

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
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** Provider for processed favicons in Tab list. */
@NullMarked
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

    /** Delegate interface for providing a favicon from a tab's web contents. */
    @FunctionalInterface
    public interface TabWebContentsFaviconDelegate {
        /** Returns the bitmap the tab has for a favicon on its web contents. */
        @Nullable
        Bitmap getBitmap(Tab tab);
    }

    /**
     * Wrapper class that holds a favicon drawable and whether recolor is allowed. Subclasses should
     * make a best effort to implement an {@link Object#equals(Object)} that will allow efficient
     * comparisons of favicon objects.
     */
    public abstract static class TabFavicon {
        private final Drawable mDefaultDrawable;
        private final Drawable mSelectedDrawable;
        private final boolean mIsRecolorAllowed;

        protected TabFavicon(
                Drawable defaultDrawable, Drawable selectedDrawable, boolean allowRecolor) {
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
        private final GURL mGurl;

        private UrlTabFavicon(
                Drawable defaultDrawable,
                Drawable selectedDrawable,
                boolean allowRecolor,
                GURL gurl) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mGurl = gurl;
        }

        @VisibleForTesting
        public UrlTabFavicon(Drawable drawable, GURL gurl) {
            this(drawable, drawable, false, gurl);
        }

        @Override
        public int hashCode() {
            return mGurl.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof UrlTabFavicon other) && Objects.equals(mGurl, other.mGurl);
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
                Drawable defaultDrawable,
                Drawable selectedDrawable,
                boolean allowRecolor,
                @StaticTabFaviconType int type) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mType = type;
        }

        @VisibleForTesting
        public ResourceTabFavicon(Drawable defaultDrawable, @StaticTabFaviconType int type) {
            this(defaultDrawable, defaultDrawable, false, type);
        }

        @Override
        public int hashCode() {
            return Integer.hashCode(mType);
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof ResourceTabFavicon other) && mType == other.mType;
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
        private @MonotonicNonNull TabFavicon mTabFavicon;

        LazyTabFaviconResolver(TabFaviconResolver resolver) {
            assert resolver != null;
            mResolver = resolver;
        }

        TabFavicon get(Context context) {
            if (mTabFavicon == null) {
                assert mResolver != null;
                mTabFavicon = assertNonNull(mResolver.resolve(context));
                mResolver = null;
            }
            return mTabFavicon;
        }
    }

    /**
     * The metadata details for a tab that has its favicon requested. This object services both real
     * {@link Tab}s and {@link SavedTabGroupTab}s. If the tab field is null, a SavedTabGroupTab is
     * being referenced. The tab field is only used for live tabs when retrieving a thumbnail via
     * the web contents state if possible.
     */
    public static class TabFaviconMetadata {
        public final @Nullable Tab tab;
        public final GURL url;
        public final boolean isIncognito;
        public final boolean isInTabGroup;

        public TabFaviconMetadata(
                @Nullable Tab tab, GURL url, boolean isIncognito, boolean isInTabGroup) {
            this.tab = tab;
            this.url = url;
            this.isIncognito = isIncognito;
            this.isInTabGroup = isInTabGroup;
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.tab, this.url, this.isIncognito, this.isInTabGroup);
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof TabFaviconMetadata other)
                    && this.tab == other.tab
                    && Objects.equals(this.url, other.url)
                    && this.isIncognito == other.isIncognito
                    && this.isInTabGroup == other.isInTabGroup;
        }
    }

    private static LazyTabFaviconResolver sRoundedGlobeFavicon;
    private static LazyTabFaviconResolver sRoundedGlobeFaviconForStrip;
    private static LazyTabFaviconResolver sRoundedGlobeFaviconIncognito;

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
    private final Context mContext;
    private final boolean mIsTabStrip;
    private final int mFaviconCornerRadius;
    private final @Nullable TabWebContentsFaviconDelegate mTabWebContentsFaviconDelegate;

    private boolean mIsInitialized;
    private @MonotonicNonNull Profile mProfile;
    private @Nullable FaviconHelper mFaviconHelper;

    /**
     * Construct the provider that provides favicons for tab list.
     *
     * @param context The context to use for accessing {@link android.content.res.Resources}
     * @param isTabStrip Indicator for whether this class provides favicons for tab strip or not.
     * @param faviconCornerRadiusId The resource Id for the favicon corner radius.
     * @param tabWebContentsFaviconDelegate An optional delegate for fetching favicons off a tab's
     *     web contents.
     */
    public TabListFaviconProvider(
            Context context,
            boolean isTabStrip,
            int faviconCornerRadiusId,
            @Nullable TabWebContentsFaviconDelegate tabWebContentsFaviconDelegate) {
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
        mTabWebContentsFaviconDelegate = tabWebContentsFaviconDelegate;

        @ColorInt
        int defaultIconColor =
                TabCardThemeUtil.getChromeOwnedFaviconTintColor(context, false, false);
        mSelectedIconColor = TabCardThemeUtil.getChromeOwnedFaviconTintColor(context, false, true);
        @ColorInt
        int incognitoIconColor =
                TabCardThemeUtil.getChromeOwnedFaviconTintColor(context, true, false);
        mIncognitoSelectedIconColor =
                TabCardThemeUtil.getChromeOwnedFaviconTintColor(context, true, true);
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

    /** Destroys the native part of {@link FaviconHelper}. */
    public void destroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
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
     * @param tab The tab to get a favicon for.
     * @return a favicon fetcher to fetch the favicon from native.
     */
    public TabFaviconFetcher getFaviconForTabFetcher(Tab tab) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                getFaviconForTabAsync(
                        new TabFaviconMetadata(
                                tab,
                                tab.getUrl(),
                                tab.isIncognitoBranded(),
                                tab.getTabGroupId() != null),
                        faviconCallback);
            }
        };
    }

    /**
     * Asynchronously get the processed favicon as a {@link Drawable}.
     *
     * @param metadata The {@link TabFaviconMetadata} of the tab to get a favicon for.
     * @param faviconCallback The callback to be serviced with the drawable when ready.
     */
    public void getFaviconDrawableForTabAsync(
            TabFaviconMetadata metadata, Callback<Drawable> faviconCallback) {
        getFaviconForTabAsync(
                metadata, tabFavicon -> faviconCallback.onResult(tabFavicon.getDefaultDrawable()));
    }

    /**
     * Create a fetcher that synchronously gets the processed favicon using the provided bitmap,
     * assuming it is not recolor allowed.
     *
     * @param icon The favicon that was received.
     * @param iconUrl The url the favicon came from.
     * @return a favicon fetcher that returns a processed version of the bitmap.
     */
    public TabFaviconFetcher getFaviconFromBitmapFetcher(Bitmap icon, GURL iconUrl) {
        Drawable processedBitmap = processBitmap(icon, mIsTabStrip);
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                faviconCallback.onResult(new UrlTabFavicon(processedBitmap, iconUrl));
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
    public TabFavicon getFaviconFromBitmap(Bitmap icon, GURL iconUrl) {
        return new UrlTabFavicon(processBitmap(icon, mIsTabStrip), iconUrl);
    }

    private @Nullable Bitmap getFaviconFromTabWebContents(Tab tab) {
        return mTabWebContentsFaviconDelegate == null
                ? null
                : mTabWebContentsFaviconDelegate.getBitmap(tab);
    }

    /**
     * Asynchronously creates a {@link TabFavicon} for a tab.
     *
     * <p>Visible for testing to override return value. Prefer {@link
     * #getFaviconDrawableForTabAsync} or {@link #getFaviconForTabFetcher}.
     *
     * <p>The following cases are handled:
     *
     * <ol>
     *   <li>NTP specialization: Returns the rounded Chrome favicon.
     *   <li>The tab's web content's already has a bitmap: Returns the bitmap as a favicon.
     *   <li>The tab is in a tab group and is not incognito: First checks the local favicon DB and
     *       falls back to a proxy service to fetch the favicon if not found.
     *   <li>Standalone tabs and incognito mode: To minimize traffic to the proxy server the local
     *       favicon image is preferred for tabs not in tab groups since these should already have
     *       been visited and will have a favicon in the local favicon database.
     * </ol>
     *
     * @param metadata The {@link TabFaviconMetadata} of the tab whose favicon is being requested.
     * @param faviconCallback The callback that requests for favicon.
     */
    @VisibleForTesting
    public void getFaviconForTabAsync(
            TabFaviconMetadata metadata, Callback<TabFavicon> faviconCallback) {
        boolean isIncognito = metadata.isIncognito;
        GURL tabUrl = metadata.url;

        // Case 1: NTP specialization.
        if (UrlUtilities.isNtpUrl(tabUrl)) {
            faviconCallback.onResult(getRoundedChromeFavicon(isIncognito));
            return;
        } else if (mFaviconHelper == null) {
            faviconCallback.onResult(getRoundedGlobeFavicon(isIncognito));
            return;
        }

        // Case 2: The Tab is live and its WebContent's already has a bitmap.
        @Nullable Bitmap webContentsBitmap =
                metadata.tab == null ? null : getFaviconFromTabWebContents(metadata.tab);
        if (webContentsBitmap != null) {
            Drawable processedBitmap = processBitmap(webContentsBitmap, mIsTabStrip);
            faviconCallback.onResult(new UrlTabFavicon(processedBitmap, tabUrl));
            return;
        }

        // Note iconUrl != tabUrl.
        FaviconImageCallback faviconImageCallback =
                (image, iconUrl) -> {
                    TabFavicon favicon;
                    if (image == null) {
                        favicon = getRoundedGlobeFavicon(isIncognito);
                    } else if (UrlUtilities.isInternalScheme(tabUrl) && !mIsTabStrip) {
                        Bitmap resizedFavicon =
                                getResizedBitmapFromDrawable(
                                        processBitmap(image, false), mDefaultFaviconSize);
                        @ColorInt
                        int iconColor =
                                isIncognito ? mIncognitoSelectedIconColor : mSelectedIconColor;
                        favicon =
                                createChromeOwnedUrlTabFavicon(
                                        resizedFavicon, 0, iconColor, true, iconUrl);
                    } else {
                        favicon = new UrlTabFavicon(processBitmap(image, mIsTabStrip), iconUrl);
                    }
                    faviconCallback.onResult(favicon);
                };

        Profile profile = getProfile(isIncognito);
        if (metadata.isInTabGroup && !isIncognito) {
            // Case 3: The tab is in a tab group and is not incognito.
            //
            // This approach first checks the local favicon DB and falls back to a proxy service to
            // fetch the favicon if not found. The proxy is only used if the profile meets certain
            // sync and sign-in conditions.
            //
            // Tabs originating from TabGroupSyncService have a higher chance of this happening
            // since the host they are for may never have been accessed on the device so the local
            // favicon database will have no fallback. However, to avoid a complex multi-part check
            // that would be repeated in native just call this method for all tab groups since they
            // are relatively rare.
            mFaviconHelper.getForeignFaviconImageForURL(
                    profile, tabUrl, mFaviconSize, faviconImageCallback);
        } else {
            // Case 4: Standalone tabs and incognito mode.
            //
            // To minimize traffic to the proxy server the local favicon image is preferred for tabs
            // not in tab groups since these should already have been visited and will have a
            // favicon in the local favicon database. There are some exceptions where this is not
            // the case e.g. low-end devices using lazily loaded background tabs. However, the
            // exceptions are rare enough it probably isn't worth the extra traffic.
            //
            // Notably in incognito mode favicons are not saved to the favicon database and the
            // proxy is not used so there is a high chance of not seeing a favicon in incognito
            // mode if Case 2 didn't provide one and there isn't a fallback with the same host in
            // the favicon database already.
            mFaviconHelper.getLocalFaviconImageForURL(
                    profile, tabUrl, mFaviconSize, faviconImageCallback);
        }
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

        int colorDefault = TabCardThemeUtil.getChromeOwnedFaviconTintColor(mContext, false, false);
        favicon.getDefaultDrawable()
                .setColorFilter(new PorterDuffColorFilter(colorDefault, PorterDuff.Mode.SRC_IN));

        if (favicon.hasSelectedState()) {
            int colorSelected =
                    TabCardThemeUtil.getChromeOwnedFaviconTintColor(mContext, false, true);
            favicon.getSelectedDrawable()
                    .setColorFilter(
                            new PorterDuffColorFilter(colorSelected, PorterDuff.Mode.SRC_IN));
        }

        return favicon;
    }

    private Profile getProfile(boolean isIncognito) {
        assert mProfile != null;
        if (!isIncognito) return mProfile;

        Profile otrProfile = mProfile.getPrimaryOtrProfile(/* createIfNeeded= */ false);
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
            return processBitmapWithBackground(
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

    private static Drawable processBitmapWithBackground(
            Context context, Bitmap bitmap, int size, int cornerRadius, int inset) {
        Drawable favicon = processBitmapNoBackground(context, bitmap, size, cornerRadius);
        Drawable circleBackground =
                AppCompatResources.getDrawable(context, R.drawable.tab_strip_favicon_circle);
        Drawable[] layers = {circleBackground, favicon};
        LayerDrawable layerDrawable = new LayerDrawable(layers);
        layerDrawable.setLayerInset(1, inset, inset, inset, inset);
        return layerDrawable;
    }

    @Initializer
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

        // Tab strip favicons do not recolor when selected.
        if (sRoundedGlobeFaviconForStrip == null) {
            sRoundedGlobeFaviconForStrip =
                    new LazyTabFaviconResolver(
                            (context) -> {
                                Drawable globeDrawable =
                                        AppCompatResources.getDrawable(
                                                context, R.drawable.ic_globe_24dp);
                                return new ResourceTabFavicon(
                                        processBitmapWithBackground(
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
                                        processBitmapWithBackground(
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
