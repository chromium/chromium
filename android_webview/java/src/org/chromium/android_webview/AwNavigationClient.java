// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;
import androidx.annotation.UiThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationState;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.PageState;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.WeakHashMap;
import java.util.function.Supplier;

import javax.annotation.concurrent.GuardedBy;

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
    @GuardedBy("mMapLock")
    private final WeakHashMap<NavigationHandle, WeakReference<AwNavigation>> mNavigationMap =
            new WeakHashMap<>();

    @GuardedBy("mMapLock")
    private final WeakHashMap<NavigationState, WeakReference<AwNavigationState>>
            mNavigationStateMap = new WeakHashMap<>();

    // Similar reason as above, but between Page and AwPage.
    @GuardedBy("mMapLock")
    private final WeakHashMap<Page, WeakReference<AwPage>> mPageMap = new WeakHashMap<>();

    @GuardedBy("mMapLock")
    private final WeakHashMap<PageState, WeakReference<AwPageState>> mPageStateMap =
            new WeakHashMap<>();

    private final Object mMapLock = new Object();

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

    @UiThread
    public void onNavigationStarted(NavigationHandle navigation) {
        ThreadUtils.assertOnUiThread();
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationStarted(awNavigation);
        }
    }

    @UiThread
    public void onNavigationRedirected(NavigationHandle navigation) {
        ThreadUtils.assertOnUiThread();
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationRedirected(awNavigation);
        }
    }

    @UiThread
    public void onNavigationCompleted(NavigationHandle navigation) {
        ThreadUtils.assertOnUiThread();
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationCompleted(awNavigation);
        }
    }

    @UiThread
    public void onNavigationVisible(NavigationHandle navigation) {
        ThreadUtils.assertOnUiThread();
        AwNavigation awNavigation = getOrUpdateAwNavigationFor(navigation);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onNavigationVisible(awNavigation);
        }
    }

    // Page.PageDeletionListener implementation
    @UiThread
    @Override
    public void onWillDeletePage(Page page) {
        ThreadUtils.assertOnUiThread();
        if (!page.isPrerendering()) {
            AwPage awPage = getAwPageFor(page);
            for (AwNavigationListener listener : mNavigationListeners) {
                listener.onPageDeleted(awPage);
            }
        }
    }

    @UiThread
    public void onPageLoadEventFired(Page page) {
        ThreadUtils.assertOnUiThread();
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPageLoadEventFired(awPage);
        }
    }

    @UiThread
    public void onPageDOMContentLoadedEventFired(Page page) {
        ThreadUtils.assertOnUiThread();
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPageDOMContentLoadedEventFired(awPage);
        }
    }

    @UiThread
    @CalledByNative
    public void onFirstContentfulPaint(Page page, long durationMs) {
        ThreadUtils.assertOnUiThread();
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onFirstContentfulPaint(awPage, durationMs);
        }
    }

    @UiThread
    @CalledByNative
    public void onLargestContentfulPaint(Page page, long durationMs) {
        ThreadUtils.assertOnUiThread();
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onLargestContentfulPaint(awPage, durationMs);
        }
    }

    @UiThread
    @CalledByNative
    public void onPerformanceMark(
            Page page, @JniType("std::string") String markName, long markTimeMs) {
        ThreadUtils.assertOnUiThread();
        AwPage awPage = getAwPageFor(page);
        for (AwNavigationListener listener : mNavigationListeners) {
            listener.onPerformanceMark(awPage, markName, markTimeMs);
        }
    }

    /**
     * A generic method which will fetch an existing wrapped Navigation/State or Page/State object
     * if it exists, if it doesn't exist or the existing WeakReference entry is invalid it will
     * compute a new Navigation/State or Page/State object.
     *
     * @param <Key> The key type of the map (e.g. NavigationHandle or Page)
     * @param <Value> The value type of the map (wrapped version of Key type)
     * @param map The map storing existing objects
     * @param key The actual key of type <Key> param
     * @param factory A supplier which creates a new object
     * @return The final AW wrapped object
     */
    private static <Key, Value> Value computeIfAbsent(
            WeakHashMap<Key, WeakReference<Value>> map, Key key, Supplier<Value> factory) {

        WeakReference<Value> ref = map.get(key);
        if (ref != null) {
            Value value = ref.get();
            if (value != null) {
                return value;
            }
        }

        Value value = factory.get();
        map.put(key, new WeakReference<>(value));
        return value;
    }

    @UiThread
    public AwNavigation getOrUpdateAwNavigationFor(NavigationHandle navigation) {
        AwPage awPage =
                navigation.getCommittedPage() == null
                        ? null
                        : getAwPageFor(navigation.getCommittedPage());
        synchronized (mMapLock) {
            AwNavigation awNavigation =
                    computeIfAbsent(
                            mNavigationMap,
                            navigation,
                            () ->
                                    new AwNavigation(
                                            navigation, awPage, this::getAwNavigationStateFor));
            awNavigation.setPage(awPage);
            return awNavigation;
        }
    }

    @AnyThread
    private AwPage getAwPageFor(Page page) {
        AwPage awPage;
        synchronized (mMapLock) {
            WeakReference<AwPage> pageRef = mPageMap.get(page);
            if (pageRef != null && pageRef.get() != null) {
                return pageRef.get();
            } else {
                awPage = new AwPage(page, this::getAwPageStateFor);
                mPageMap.put(page, new WeakReference<>(awPage));
            }
        }
        // getAwPageFor can be called from any thread, but isPrerendering and
        // setPageDeletionListener is not thread safe and must only be called from the UI
        // thread, most of the time this will only be called on the UI thread and no overhead is
        // introduced due to a new AwPage only being created by the @UiThread callbacks above but
        // can be called from a background thread when a developer calls
        // NavigationState#snapshotState and the corresponding AwPage is no longer valid in the
        // mPageMap.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert !page.isPrerendering();
                    page.setPageDeletionListener(this);
                });
        return awPage;
    }

    @AnyThread
    public AwNavigationState getAwNavigationStateFor(
            NavigationHandle navigation, AwNavigation awNavigation) {
        NavigationState navigationState = navigation.getMostRecentNavigationState();

        @Nullable AwPageState awPageState;

        @Nullable PageState pageState = navigationState.getCommittedPageState();
        if (pageState != null) {
            AwPage awPage = getAwPageFor(pageState.getPage());
            awPageState = getAwPageStateFor(awPage, pageState);
        } else {
            awPageState = null;
        }

        synchronized (mMapLock) {
            return computeIfAbsent(
                    mNavigationStateMap,
                    navigationState,
                    () -> new AwNavigationState(navigationState, awNavigation, awPageState));
        }
    }

    @AnyThread
    public AwPageState getAwPageStateFor(AwPage awPage, PageState pageState) {
        synchronized (mMapLock) {
            return computeIfAbsent(
                    mPageStateMap, pageState, () -> new AwPageState(pageState, awPage));
        }
    }
}
