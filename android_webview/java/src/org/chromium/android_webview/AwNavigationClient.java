// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.WeakHashMap;

/** Routes notifications about navigations from AwWebContentsObserver and AwContents to listeners */
@NullMarked
@Lifetime.WebView
public class AwNavigationClient implements Page.PageDeletionListener {
    private final List<AwNavigationListener> mNavigationListeners = new ArrayList<>();

    // Maps a NavigationHandle to its associated AwNavigation object. Since the AwNavigation object
    // subclasses AwSupportLibIsomorphic in order to hold onto a reference to the client-side
    // object and we want to keep it stable (so that apps can use the object itself as implicit
    // IDs), we need to keep a mapping. Some important points:
    // - If the app keeps a reference to the app-facing navigation object around, that object will
    //   reference the AwNavigation, which references the NavigationHandle, and so all these
    //   objects will be kept alive: the map will continue to associate the two so that future
    //   callbacks use the same object, and the app can also continue calling the getters even
    //   after //content's native code is no longer keeping the handle around.
    // - If the app doesn't keep a reference to the app-facing navigation object, then the
    //   NavigationHandle will be kept alive by //content for as long as the navigation is in
    //   progress, which will also keep the entry in the hashmap alive, but because the hashmap
    //   value is a weak reference then this will not keep the AwNavigation alive and it might get
    //   GCed in between callbacks; we would have to create a new AwNavigation wrapper if that
    //   happens to call the next callback which is not ideal for performance but doesn't affect
    //   the app-visible behavior of the API much: they didn't keep a reference to the
    //   navigation around the first time and so they can't tell whether the second time is the
    //   same object or not.
    // - The app unfortunately can tell if they store a weak reference to the navigation object,
    //   but there's no need for them to do that here: strongly referencing the object doesn't
    //   leak the WebView or anything.
    private final WeakHashMap<NavigationHandle, WeakReference<AwNavigation>> mNavigationMap =
            new WeakHashMap<>();
    // Similar reason as above, but between Page and AwPage.
    private final WeakHashMap<Page, WeakReference<AwPage>> mPageMap = new WeakHashMap<>();

    /**
     * Adds a listener to the list. The listener will not be added if it has already been added to
     * the list.
     *
     * @return true if the listener was added to the list.
     */
    public boolean addListener(AwNavigationListener listener) {
        if (mNavigationListeners.contains(listener)) {
            return false;
        }
        return mNavigationListeners.add(listener);
    }

    public void removeListener(AwNavigationListener listener) {
        mNavigationListeners.remove(listener);
    }

    /**
     * Legacy method to support deprecated method
     * SupportLibWebViewChromium:getWebViewNavigationClient.
     */
    @Deprecated
    public @Nullable AwNavigationListener getFirstListener() {
        return mNavigationListeners.isEmpty() ? null : mNavigationListeners.get(0);
    }

    /**
     * Legacy method to support deprecated method
     * SupportLibWebViewChromium:setWebViewNavigationClient.
     *
     * @deprecated {@link #addListener(AwNavigationListener listener)} instead.
     */
    @Deprecated
    public void clearAndSetListener(AwNavigationListener listener) {
        mNavigationListeners.clear();
        mNavigationListeners.add(listener);
    }

    public void onNavigationStarted(NavigationHandle navigation) {
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationStarted(awNavigation);
        }
    }

    public void onNavigationRedirected(NavigationHandle navigation) {
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationRedirected(awNavigation);
        }
    }

    public void onNavigationCompleted(NavigationHandle navigation) {
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationCompleted(awNavigation);
        }
    }

    // Page.PageDeletionListener implementation
    @Override
    public void onWillDeletePage(Page page) {
        if (!page.isPrerendering()) {
            AwPage awPage = getAwPageFor(page);
            for (AwNavigationListener listener : mNavigationListeners) {
                listener.onPageDeleted(awPage);
            }
        }
    }

    public void onPageLoadEventFired(Page page) {
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPageLoadEventFired(awPage);
        }
    }

    public void onPageDOMContentLoadedEventFired(Page page) {
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPageDOMContentLoadedEventFired(awPage);
        }
    }

    public void onFirstContentfulPaint(Page page, long durationUs) {
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onFirstContentfulPaint(awPage, durationUs);
        }
    }

    public void onPerformanceMark(Page page, String markName, long markTimeMs) {
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPerformanceMark(awPage, markName, markTimeMs);
        }
    }

    public AwNavigation getOrUpdateAwNavigationFor(NavigationHandle navigation) {
        WeakReference<AwNavigation> awNavigationRef = mNavigationMap.get(navigation);
        AwPage awPage =
                navigation.getCommittedPage() == null
                        ? null
                        : getAwPageFor(navigation.getCommittedPage());
        if (awNavigationRef != null) {
            AwNavigation awNavigation = awNavigationRef.get();
            if (awNavigation != null) {
                // We're reusing an existing AwNavigation, but the AwPage associated with it might
                // have changed (e.g. if the AwNavigation was created at navigation start it will
                // be constructed with a null page value, but then it commits a page and needs to
                // be updated).
                awNavigation.setPage(awPage);
                return awNavigation;
            }
        }
        AwNavigation awNavigation = new AwNavigation(navigation, awPage);
        mNavigationMap.put(navigation, new WeakReference<>(awNavigation));
        return awNavigation;
    }

    private AwPage getAwPageFor(Page page) {
        WeakReference<AwPage> awPageRef = mPageMap.get(page);
        if (awPageRef != null) {
            AwPage awPage = awPageRef.get();
            if (awPage != null) {
                return awPage;
            }
        }
        AwPage awPage = new AwPage(page);
        // We only keep track of pages that have been the primary page (either the current primary
        // page, or a previously primary but now bfcached / pending deletion page).
        assert !awPage.isPrerendering();
        // Make sure we always track deletion of a non-prerendering page.
        page.setPageDeletionListener(this);
        mPageMap.put(page, new WeakReference<>(awPage));
        return awPage;
    }
}
