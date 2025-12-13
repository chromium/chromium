// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.host_zoom;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.accessibility.ZoomEventsObserver;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.SiteZoomInfo;

/**
 * Listens for zoom level changes from the native HostZoomMap and notifies Java observers.
 *
 * <p>This class serves as an observer hub or multiplexer. Instead of every Java UI component
 * creating its own native observer to listen for zoom events (which would be inefficient and
 * complex), this class creates a single native observer per Profile. It then fans out the
 * notifications to any number of Java-side observers. This encapsulates the JNI logic, provides a
 * clean and safe API for clients, and maintains a good architectural boundary between the //chrome
 * and //content layers.
 *
 * <p>This class's lifecycle is tied to a {@link Profile}. Instances are created and managed by
 * {@link HostZoomListenerFactory} to ensure that there is only one instance per profile. The
 * factory is responsible for calling {@link #destroy()} when the profile is destroyed. We also do
 * not mind if this class ends up leaky, as it is meant to live for as long as the browser process
 * does.
 *
 * <p>The same HostZoomListener instance is shared between a regular profile and its corresponding
 * incognito profile. This is because zoom level settings are not separated for incognito mode; they
 * are inherited from the original profile.
 *
 * <p>UI components that need to react to zoom changes should:
 *
 * <ol>
 *   <li>Get an instance of this class via {@link HostZoomListenerFactory#getForProfile(Profile)}.
 *   <li>Add an observer via {@link #addObserver(ZoomEventsObserver)}.
 *   <li>Remove the observer via {@link #removeObserver(ZoomEventsObserver)} when the component is
 *       destroyed to prevent memory leaks.
 * </ol>
 */
@NullMarked
public class HostZoomListener implements Destroyable {

    private final BrowserContextHandle mBrowserContextHandle;
    private final ObserverList<ZoomEventsObserver> mObservers = new ObserverList();
    private long mNativeHostZoomLevelListenerKey;

    /**
     * @param browserContextHandle The BrowserContextHandle to use for the zoom manager.
     */
    /* package */ HostZoomListener(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
        mNativeHostZoomLevelListenerKey =
                addZoomLevelObserver(mBrowserContextHandle, this::onZoomLevelChanged);
        assert mNativeHostZoomLevelListenerKey != -1
                : "Failed to create native HostZoomMap observer.";
    }

    @Override
    public void destroy() {
        if (mNativeHostZoomLevelListenerKey != 0) {
            removeZoomLevelObserver(mBrowserContextHandle, mNativeHostZoomLevelListenerKey);
            mNativeHostZoomLevelListenerKey = 0;
        }
        mObservers.clear();
    }

    // Java observer methods.

    /**
     * Adds an observer for zoom level changes.
     *
     * @param observer The observer to add.
     */
    public void addObserver(ZoomEventsObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(ZoomEventsObserver observer) {
        mObservers.removeObserver(observer);
    }

    // Native observer methods.

    @CalledByNative
    private void onZoomLevelChanged(SiteZoomInfo siteZoomInfo) {
        String host = siteZoomInfo.host;
        double zoomLevel = siteZoomInfo.zoomLevel;
        for (ZoomEventsObserver observer : mObservers) {
            observer.onZoomLevelChanged(host, zoomLevel);
        }
    }

    /**
     * Creates a zoom level observer for the given browser context.
     *
     * <p>The returned pointer points to the key for the native subscription object. It must be
     * passed to {@link #removeZoomLevelObserver} when the observer is no longer needed.
     *
     * @param browserContextHandle The BrowserContextHandle to observe.
     * @param callback The callback to be invoked when the zoom level changes.
     * @return A pointer to the native subscription object.
     */
    private long addZoomLevelObserver(
            BrowserContextHandle browserContextHandle, Callback<SiteZoomInfo> callback) {
        return HostZoomMap.addZoomLevelObserver(browserContextHandle, callback);
    }

    /**
     * Destroys the zoom level observer.
     *
     * @param subscriptionPtr A pointer to the native subscription object.
     */
    private void removeZoomLevelObserver(
            BrowserContextHandle browserContextHandle, long subscriptionPtr) {
        HostZoomMap.removeZoomLevelObserver(browserContextHandle, subscriptionPtr);
    }

    long getNativeHostZoomLevelListenerKeyForTesting() {
        return mNativeHostZoomLevelListenerKey;
    }
}
