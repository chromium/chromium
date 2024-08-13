// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Fetches a favicon for active WebContents in a Tab. */
public class TabFavicon extends TabWebContentsUserData {
    private static final Class<TabFavicon> USER_DATA_KEY = TabFavicon.class;

    private final TabImpl mTab;
    private final long mNativeTabFavicon;

    /**
     * The size in pixels at which favicons will be drawn. Ideally mFavicon will have this size to
     * avoid scaling artifacts.
     */
    private final int mIdealFaviconSize;

    // The ideal favicon size for navigation transitions, in DIP.
    private final int mNavigationTransitionsIdealFaviconSize;
    // The current favicon width and height for navigation transitions.
    private int mNavigationTransitionsFaviconWidth;
    private int mNavigationTransitionsFaviconHeight;
    // The URL of the tab when the favicon was fetch for navigation transitions.
    private GURL mFaviconTabUrlForNavigationTransition;

    private Bitmap mFavicon;
    private int mFaviconWidth;
    private int mFaviconHeight;
    // The URL of the tab when mFavicon was fetched.
    private GURL mFaviconTabUrl;

    static TabFavicon from(Tab tab) {
        TabFavicon favicon = get(tab);
        if (favicon == null) {
            favicon = tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabFavicon(tab));
        }
        return favicon;
    }

    private static TabFavicon get(Tab tab) {
        if (tab == null || !tab.isInitialized()) return null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * @param tab Tab containing the web contents's favicon.
     * @return {@link Bitmap} of the favicon.
     */
    public static @Nullable Bitmap getBitmap(Tab tab) {
        TabFavicon tabFavicon = get(tab);
        return tabFavicon != null ? tabFavicon.getFavicon() : null;
    }

    private TabFavicon(Tab tab) {
        super(tab);
        mTab = (TabImpl) tab;
        Resources resources = mTab.getThemedApplicationContext().getResources();
        mIdealFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        mNavigationTransitionsIdealFaviconSize =
                resources.getDimensionPixelSize(R.dimen.navigation_transitions_favicon_size);
        mNativeTabFavicon =
                TabFaviconJni.get().init(TabFavicon.this, mNavigationTransitionsIdealFaviconSize);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        TabFaviconJni.get().setWebContents(mNativeTabFavicon, TabFavicon.this, webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        TabFaviconJni.get().resetWebContents(mNativeTabFavicon, TabFavicon.this);
    }

    @Override
    public void destroyInternal() {
        TabFaviconJni.get().onDestroyed(mNativeTabFavicon, TabFavicon.this);
    }

    /**
     * @return The bitmap of the favicon scaled to 16x16dp. null if no favicon
     *         is specified or it requires the default favicon.
     */
    private Bitmap getFavicon() {
        // If we have no content or a native page, return null.
        if (mTab.isNativePage() || mTab.getWebContents() == null) return null;

        // Use the cached favicon only if the page wasn't changed.
        if (mFavicon != null && mFaviconTabUrl != null && mFaviconTabUrl.equals(mTab.getUrl())) {
            return mFavicon;
        }

        return TabFaviconJni.get().getFavicon(mNativeTabFavicon, TabFavicon.this);
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

    private boolean pageUrlChanged() {
        GURL currentTabUrl = mTab.getUrl();
        return !currentTabUrl.equals(mFaviconTabUrl);
    }

    private boolean pageUrlChangedForNavigationTransitions() {
        GURL currentTabUrl = mTab.getUrl();
        return !currentTabUrl.equals(mFaviconTabUrlForNavigationTransition);
    }

    @CalledByNative
    @VisibleForTesting
    void onFaviconAvailable(Bitmap icon, GURL iconUrl) {
        assert icon != null;
        // Bitmap#createScaledBitmap will return the original bitmap if it is already
        // |mIdealFaviconSize|x|mIdealFaviconSize| DP.
        mFavicon = Bitmap.createScaledBitmap(icon, mIdealFaviconSize, mIdealFaviconSize, true);
        mFaviconWidth = icon.getWidth();
        mFaviconHeight = icon.getHeight();
        mFaviconTabUrl = mTab.getUrl();
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onFaviconUpdated(mTab, icon, iconUrl);
    }

    @CalledByNative
    @VisibleForTesting
    boolean shouldUpdateFaviconForBrowserUI(int newIconWidth, int newIconHeight) {
        return pageUrlChanged()
                || isBetterFavicon(
                        mFaviconWidth,
                        mFaviconHeight,
                        newIconWidth,
                        newIconHeight,
                        mIdealFaviconSize);
    }

    @CalledByNative
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

    @NativeMethods
    interface Natives {
        long init(TabFavicon caller, int navigaionTransitionFaviconSize);

        void onDestroyed(long nativeTabFavicon, TabFavicon caller);

        void setWebContents(long nativeTabFavicon, TabFavicon caller, WebContents webContents);

        void resetWebContents(long nativeTabFavicon, TabFavicon caller);

        Bitmap getFavicon(long nativeTabFavicon, TabFavicon caller);
    }
}
