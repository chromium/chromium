// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.url_constants.UrlOverrideUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Fetches a favicon for active WebContents in a Tab. */
@NullMarked
public class TabFavicon extends TabWebContentsUserData {
    private static final Class<TabFavicon> USER_DATA_KEY = TabFavicon.class;
    private static @Nullable TabFavicon sInstanceForTesting;

    private final TabImpl mTab;
    private long mNativeTabFavicon;

    /**
     * The size in pixels at which favicons will be drawn. Ideally mFavicon will have this size to
     * avoid scaling artifacts.
     */
    private int mIdealFaviconSize;

    // The ideal favicon size for navigation transitions, in DIP.
    private final int mNavigationTransitionsIdealFaviconSize;
    private final List<Promise<Bitmap>> mPendingPromises = new ArrayList<>();

    // The current favicon width and height for navigation transitions.
    private int mNavigationTransitionsFaviconWidth;
    private int mNavigationTransitionsFaviconHeight;
    // The URL of the tab when the favicon was fetch for navigation transitions.
    private @Nullable GURL mFaviconTabUrlForNavigationTransition;

    private @Nullable Bitmap mFavicon;
    private int mFaviconWidth;
    private int mFaviconHeight;
    // The URL of the tab when mFavicon was fetched.
    private @Nullable GURL mFaviconTabUrl;
    // The last committed URL of the primary main frame when mFavicon was fetched. mFaviconTabUrl
    // is the *visible* URL, which can stay the same across a cross-document navigation (e.g.
    // chrome://newtab resolving to a different New Tab Page after the default search engine
    // changed), so on its own it can't tell whether mFavicon still belongs to the current document.
    private @Nullable GURL mFaviconCommittedUrl;
    private boolean mIsFaviconFallback;
    @VisibleForTesting @Nullable FaviconHelper mFaviconHelper;

    /**
     * Returns the TabFavicon object creating it if necessary.
     *
     * @param Tab tab the tab to get or create the TabFavicon from.
     * @return The tab favicon.
     */
    @CalledByNative
    public static @Nullable TabFavicon from(@Nullable @JniType("TabAndroid*") Tab tab) {
        if (sInstanceForTesting != null) return sInstanceForTesting;

        if (!TabUtils.isValid(tab)) return null;
        if (!(tab instanceof TabImpl)) return null;

        TabFavicon favicon = get(tab);
        if (favicon == null) {
            favicon = tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabFavicon(tab));
        }
        return favicon;
    }

    private static @Nullable TabFavicon get(Tab tab) {
        if (sInstanceForTesting != null) return sInstanceForTesting;

        if (!TabUtils.isValid(tab)) return null;

        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * @param tab Tab containing the web contents's favicon.
     * @return {@link Bitmap} of the favicon.
     */
    public static @Nullable Bitmap getBitmap(Tab tab) {
        return getBitmapWithFallback(tab, /* allowFallback= */ false);
    }

    /**
     * @param tab Tab containing the web contents's favicon.
     * @param allowFallback Whether to allow returning the fallback from the local favicon db.
     * @return {@link Bitmap} of the favicon.
     */
    @CalledByNative
    public static @Nullable Bitmap getBitmapWithFallback(
            @JniType("TabAndroid*") Tab tab, boolean allowFallback) {
        TabFavicon tabFavicon = get(tab);
        return tabFavicon != null ? tabFavicon.getFavicon(allowFallback) : null;
    }

    private TabFavicon(Tab tab) {
        super(tab);
        mTab = (TabImpl) tab;
        mIdealFaviconSize = getIdealFaviconSize();
        mNavigationTransitionsIdealFaviconSize = getNavigationTransitionsIdealFaviconSize();
        mNativeTabFavicon = TabFaviconJni.get().init(tab, mNavigationTransitionsIdealFaviconSize);
        if (tab.getWebContents() != null) {
            if (ChromeFeatureList.sClankStartupTabOptimizations.isEnabled()) {
                // Call onContentChanged() rather than initWebContents() directly so
                // TabWebContentsUserData#mWebContents is populated and the subsequent
                // notifyContentChanged() call in TabImpl#initWebContents() is a no-op.
                onContentChanged(tab);
            } else {
                initWebContents(tab.getWebContents());
            }
        }
    }

    private int getIdealFaviconSize() {
        return mTab.getContext().getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
    }

    private int getNavigationTransitionsIdealFaviconSize() {
        return mTab.getContext()
                .getResources()
                .getDimensionPixelSize(R.dimen.navigation_transitions_favicon_size);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        if (mNativeTabFavicon != 0) {
            TabFaviconJni.get().setWebContents(mNativeTabFavicon, webContents);
        }
    }

    @Override
    public void cleanupWebContents(@Nullable WebContents webContents) {
        if (mNativeTabFavicon != 0) {
            TabFaviconJni.get().resetWebContents(mNativeTabFavicon);
        }
    }

    @Override
    public void destroyInternal() {
        for (var pendingPromise : mPendingPromises) {
            pendingPromise.reject(new Exception("Tab destroyed"));
        }
        mPendingPromises.clear();
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
        if (mNativeTabFavicon != 0) {
            TabFaviconJni.get().onDestroyed(mNativeTabFavicon);
            mNativeTabFavicon = 0;
        }
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        maybeInvalidateFaviconForNewDocument(navigation);
    }

    /**
     * Returns the bitmap of the favicon scaled to 16x16dp. null if no favicon is specified or it
     * requires the default favicon.
     */
    @VisibleForTesting
    public @Nullable Bitmap getFavicon() {
        return getFavicon(/* allowFallback= */ false);
    }

    /**
     * Gets the bitmap of the favicon scaled to 16x16dp.
     *
     * @param allowFallback Whether to allow returning a result from the favicon db.
     * @return bitmap of the favicon or null if unspecified or unavailable.
     */
    public @Nullable Bitmap getFavicon(boolean allowFallback) {
        if (mNativeTabFavicon == 0) return null;

        // Native pages never return anything.
        if (mTab.isNativePage()) {
            return null;
        }

        boolean isUrlSame = mFaviconTabUrl != null && mFaviconTabUrl.equals(mTab.getUrl());
        // Use the cached favicon only if the page wasn't changed and the size is correct. Skip this
        // if the favicon is a fallback and we don't allow fallbacks.
        if (mFavicon != null
                && isUrlSame
                && mFavicon.getWidth() == getIdealFaviconSize()
                && (!mIsFaviconFallback || allowFallback)) {
            return mFavicon;
        }

        // If we have no content, or have a pending navigation, return null.
        if (mTab.getWebContents() == null || mTab.getPendingLoadParams() != null) {
            return null;
        }

        return TabFaviconJni.get().getFavicon(mNativeTabFavicon);
    }

    /**
     * Requests a favicon for the tab. This will be fulfilled immediately if the favicon is already
     * available. Otherwise falls back to fetching a favicon from the local favicon db
     */
    public Promise<Bitmap> getFaviconOrFallback() {
        Promise<Bitmap> promise = new Promise<>();
        if (mNativeTabFavicon == 0 || mTab.isNativePage()) {
            promise.reject(new Exception("Not eligible for favicon"));
            return promise;
        }
        Bitmap cached = getFavicon(/* allowFallback= */ true);
        if (cached != null) {
            promise.fulfill(cached);
            return promise;
        }

        if (mFaviconHelper == null) {
            mFaviconHelper = new FaviconHelper();
        }
        int desiredSize = getIdealFaviconSize();
        final GURL queryUrl = mTab.getUrl();
        mPendingPromises.add(promise);
        boolean success =
                mFaviconHelper.getLocalFaviconImageForURL(
                        mTab.getProfile(),
                        queryUrl,
                        desiredSize,
                        /* fallbackToHost= */ false,
                        new FaviconHelper.FaviconImageCallback() {
                            @Override
                            public void onFaviconAvailable(Bitmap image, GURL iconUrl) {
                                if (!promise.isPending()) return;
                                mPendingPromises.remove(promise);

                                if (!queryUrl.equals(mTab.getUrl())) {
                                    promise.reject(new Exception("URL changed during DB fetch."));
                                    return;
                                }

                                if (image != null
                                        && image.getWidth() != 0
                                        && image.getHeight() != 0) {
                                    TabFavicon.this.onFaviconAvailable(
                                            image, iconUrl, /* isFallback= */ true);
                                }

                                if (mFavicon != null && !TabFavicon.this.pageUrlChanged()) {
                                    promise.fulfill(mFavicon);
                                } else {
                                    promise.reject(new Exception("No favicon after DB fetch."));
                                }
                            }
                        });
        if (!success) {
            mPendingPromises.remove(promise);
            promise.reject(new Exception("Failed to query DB"));
        }
        return promise;
    }

    /**
     * @param currentWidth current favicon's width.
     * @param currentHeight current favicon's height.
     * @param width new favicon's width.
     * @param height new favicon's height.
     * @param idealFaviconSize the size of the ideal favicon (a square favicon).
     * @return true iff the new favicon should replace the current one.
     */
    private static boolean isBetterFavicon(
            int currentWidth, int currentHeight, int width, int height, int idealFaviconSize) {
        assert width >= 0 && height >= 0;

        if (isIdealFaviconSize(idealFaviconSize, width, height)) return true;

        // The page may be dynamically updating its URL, let it through.
        if (currentWidth == width && currentHeight == height) return true;

        // Prefer square favicons over rectangular ones
        if (currentWidth != currentHeight && width == height) return true;
        if (currentWidth == currentHeight && width != height) return false;

        // Do not update favicon if it's already at least as big as the ideal size in both dimens
        if (currentWidth >= idealFaviconSize && currentHeight >= idealFaviconSize) return false;

        // Update favicon if the new one is larger in one dimen, but not smaller in the other
        return (width > currentWidth && !(height < currentHeight))
                || (!(width < currentWidth) && height > currentHeight);
    }

    private static boolean isIdealFaviconSize(int idealFaviconSize, int width, int height) {
        return width == idealFaviconSize && height == idealFaviconSize;
    }

    boolean pageUrlChanged() {
        GURL currentTabUrl = mTab.getUrl();
        return !currentTabUrl.equals(mFaviconTabUrl);
    }

    private boolean pageUrlChangedForNavigationTransitions() {
        GURL currentTabUrl = mTab.getUrl();
        return !currentTabUrl.equals(mFaviconTabUrlForNavigationTransition);
    }

    /** Returns the last committed URL of the primary main frame, or null if there is none. */
    private @Nullable GURL getLastCommittedMainFrameUrl() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return null;
        RenderFrameHost mainFrame = webContents.getMainFrame();
        return mainFrame != null ? mainFrame.getLastCommittedURL() : null;
    }

    /**
     * Drops the cached favicon if {@code navigation} committed a different document while the
     * visible URL stayed the same, so that {@link #getFavicon(boolean)} doesn't keep serving the
     * previous document's favicon. This happens for chrome://newtab, whose visible URL doesn't
     * change when it resolves to a different New Tab Page (e.g. after the default search engine
     * changed from a third-party engine to Google); for every other navigation the visible URL
     * changes too and the cache is already bypassed by {@link #pageUrlChanged()}.
     *
     * <p>Only the WebUI NTP keeps one visible URL across different documents, so this is limited to
     * configurations where chrome://newtab is served by the WebUI NTP.
     */
    @VisibleForTesting
    void maybeInvalidateFaviconForNewDocument(NavigationHandle navigation) {
        if (!UrlOverrideUtils.isWebUiNtpOverrideEnabled()) return;
        if (mFavicon == null || !navigation.hasCommitted() || navigation.isSameDocument()) {
            return;
        }
        if (pageUrlChanged()) return;
        if (mFaviconCommittedUrl == null || mFaviconCommittedUrl.equals(navigation.getUrl())) {
            return;
        }
        mFavicon = null;
        mFaviconWidth = 0;
        mFaviconHeight = 0;
        mFaviconTabUrl = null;
        mFaviconCommittedUrl = null;
        mIsFaviconFallback = false;
        // Tell observers (e.g. the tab strip) to re-query the favicon, as the new document may
        // never provide one of its own.
        for (TabObserver observer : mTab.getTabObservers()) {
            observer.onFaviconUpdated(mTab, null, null);
        }
    }

    @CalledByNative
    private static void onFaviconAvailable(
            @JniType("TabAndroid*") Tab tab, Bitmap icon, @JniType("GURL") GURL iconUrl) {
        TabFavicon tabFavicon = get(tab);
        if (tabFavicon == null) {
            return;
        }
        tabFavicon.onFaviconAvailable(icon, iconUrl, /* isFallback= */ false);
    }

    @VisibleForTesting
    void onFaviconAvailable(Bitmap icon, GURL iconUrl, boolean isFallback) {
        assert icon != null;
        // There is already a non-fallback favicon available, prefer that.
        if (isFallback
                && mFavicon != null
                && !mIsFaviconFallback
                && mFaviconTabUrl != null
                && mFaviconTabUrl.equals(mTab.getUrl())) {
            return;
        }
        mIdealFaviconSize = getIdealFaviconSize();
        // Bitmap#createScaledBitmap will return the original bitmap if it is already
        // |mIdealFaviconSize|x|mIdealFaviconSize| DP.
        mFavicon = Bitmap.createScaledBitmap(icon, mIdealFaviconSize, mIdealFaviconSize, true);
        mFaviconWidth = icon.getWidth();
        mFaviconHeight = icon.getHeight();
        mFaviconTabUrl = mTab.getUrl();
        mFaviconCommittedUrl = getLastCommittedMainFrameUrl();
        mIsFaviconFallback = isFallback;
        for (TabObserver observer : mTab.getTabObservers()) {
            observer.onFaviconUpdated(mTab, icon, iconUrl);
        }
    }

    @CalledByNative
    private void getFaviconOrFallback(JniOnceCallback<@Nullable Bitmap> callback) {
        getFaviconOrFallback()
                .then(result -> callback.onResult(result), exception -> callback.onResult(null));
    }

    @CalledByNative
    private static boolean shouldUpdateFaviconForBrowserUi(
            @JniType("TabAndroid*") Tab tab, int newIconWidth, int newIconHeight) {
        TabFavicon tabFavicon = get(tab);
        if (tabFavicon == null) {
            return false;
        }
        return tabFavicon.shouldUpdateFaviconForBrowserUi(newIconWidth, newIconHeight);
    }

    @VisibleForTesting
    boolean shouldUpdateFaviconForBrowserUi(int newIconWidth, int newIconHeight) {
        mIdealFaviconSize = getIdealFaviconSize();
        return pageUrlChanged()
                || isBetterFavicon(
                        mFaviconWidth,
                        mFaviconHeight,
                        newIconWidth,
                        newIconHeight,
                        mIdealFaviconSize);
    }

    @CalledByNative
    private static boolean shouldUpdateFaviconForNavigationTransitions(
            @JniType("TabAndroid*") Tab tab, int newIconWidth, int newIconHeight) {
        TabFavicon tabFavicon = get(tab);
        if (tabFavicon == null) {
            return false;
        }
        return tabFavicon.shouldUpdateFaviconForNavigationTransitions(newIconWidth, newIconHeight);
    }

    private boolean shouldUpdateFaviconForNavigationTransitions(
            int newIconWidth, int newIconHeight) {
        boolean shouldUpdate =
                pageUrlChangedForNavigationTransitions()
                        || isBetterFavicon(
                                mNavigationTransitionsFaviconWidth,
                                mNavigationTransitionsFaviconHeight,
                                newIconWidth,
                                newIconHeight,
                                mNavigationTransitionsIdealFaviconSize);
        if (shouldUpdate) {
            mNavigationTransitionsFaviconWidth = newIconWidth;
            mNavigationTransitionsFaviconHeight = newIconHeight;
            mFaviconTabUrlForNavigationTransition = mTab.getUrl();
        }
        return shouldUpdate;
    }

    @CalledByNative
    @VisibleForTesting
    static long getNativePtrForTab(@JniType("TabAndroid*") Tab tab) {
        TabFavicon tabFavicon = from(tab);
        return tabFavicon != null ? tabFavicon.mNativeTabFavicon : 0;
    }

    public static void setInstanceForTesting(TabFavicon instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("TabAndroid*") Tab tab, int navigationTransitionFaviconSize);

        void onDestroyed(long nativeTabFavicon);

        void setWebContents(
                long nativeTabFavicon, @JniType("content::WebContents*") WebContents webContents);

        void resetWebContents(long nativeTabFavicon);

        @Nullable Bitmap getFavicon(long nativeTabFavicon);
    }
}
