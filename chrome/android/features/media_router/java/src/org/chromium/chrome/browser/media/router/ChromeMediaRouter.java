// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.support.v7.media.MediaRouter;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.media.router.caf.CafMediaRouteProvider;
import org.chromium.chrome.browser.media.router.caf.remoting.CafRemotingMediaRouteProvider;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Implements the JNI interface called from the C++ Media Router implementation on Android.
 * Owns a list of {@link MediaRouteProvider} implementations and dispatches native calls to them.
 */
@JNINamespace("media_router")
public class ChromeMediaRouter implements MediaRouteManager {
    private static final String TAG = "MediaRouter";
    private static final int MIN_GOOGLE_PLAY_SERVICES_APK_VERSION = 12600000;

    private static MediaRouteProvider.Factory sRouteProviderFactory =
            new MediaRouteProvider.Factory() {
                @Override
                public void addProviders(MediaRouteManager manager) {
                    int googleApiAvailabilityResult =
                            AppHooks.get().isGoogleApiAvailableWithMinApkVersion(
                                    MIN_GOOGLE_PLAY_SERVICES_APK_VERSION);
                    if (googleApiAvailabilityResult != ConnectionResult.SUCCESS) {
                        GoogleApiAvailability.getInstance().showErrorNotification(
                                ContextUtils.getApplicationContext(), googleApiAvailabilityResult);
                        return;
                    }
                    MediaRouteProvider cafProvider = CafMediaRouteProvider.create(manager);
                    manager.addMediaRouteProvider(cafProvider);
                    MediaRouteProvider remotingProvider =
                            CafRemotingMediaRouteProvider.create(manager);
                    manager.addMediaRouteProvider(remotingProvider);
                }
            };

    // The pointer to the native object. Can be null during tests, or when the
    // native object has been destroyed.
    private long mNativeMediaRouterAndroidBridge;
    private final List<MediaRouteProvider> mRouteProviders = new ArrayList<MediaRouteProvider>();
    private final Map<String, MediaRouteProvider> mRouteIdsToProviders =
            new HashMap<String, MediaRouteProvider>();
    private final Map<String, Map<MediaRouteProvider, List<MediaSink>>> mSinksPerSourcePerProvider =
            new HashMap<String, Map<MediaRouteProvider, List<MediaSink>>>();
    private final Map<String, List<MediaSink>> mSinksPerSource =
            new HashMap<String, List<MediaSink>>();
    private static boolean sAndroidMediaRouterSetForTest;
    private static MediaRouter sAndroidMediaRouterForTest;

    @VisibleForTesting
    public static void setAndroidMediaRouterForTest(MediaRouter router) {
        sAndroidMediaRouterSetForTest = true;
        sAndroidMediaRouterForTest = router;
    }

    @VisibleForTesting
    public static void setRouteProviderFactoryForTest(MediaRouteProvider.Factory factory) {
        sRouteProviderFactory = factory;
    }

    @VisibleForTesting
    protected List<MediaRouteProvider> getRouteProvidersForTest() {
        return mRouteProviders;
    }

    @VisibleForTesting
    protected Map<String, MediaRouteProvider> getRouteIdsToProvidersForTest() {
        return mRouteIdsToProviders;
    }

    @VisibleForTesting
    protected Map<String, Map<MediaRouteProvider, List<MediaSink>>>
    getSinksPerSourcePerProviderForTest() {
        return mSinksPerSourcePerProvider;
    }

    @VisibleForTesting
    protected Map<String, List<MediaSink>> getSinksPerSourceForTest() {
        return mSinksPerSource;
    }

    /**
     * Obtains the {@link MediaRouter} instance given the application context.
     * @return Null if the media router API is not supported, the service instance otherwise.
     */
    @Nullable
    public static MediaRouter getAndroidMediaRouter() {
        if (sAndroidMediaRouterSetForTest) return sAndroidMediaRouterForTest;

        // Some manufacturers have an implementation that causes StrictMode
        // violations. See https://crbug.com/818325.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // Pre-MR1 versions of JB do not have the complete MediaRouter APIs,
            // so getting the MediaRouter instance will throw an exception.
            return MediaRouter.getInstance(ContextUtils.getApplicationContext());
        } catch (NoSuchMethodError e) {
            return null;
        } catch (NoClassDefFoundError e) {
            // TODO(mlamouri): happens with Robolectric.
            return null;
        }
    }

    @Override
    public void addMediaRouteProvider(MediaRouteProvider provider) {
        mRouteProviders.add(provider);
    }

    @Override
    public void onSinksReceived(
            String sourceId, MediaRouteProvider provider, List<MediaSink> sinks) {
        if (!mSinksPerSourcePerProvider.containsKey(sourceId)) {
            mSinksPerSourcePerProvider.put(
                    sourceId, new HashMap<MediaRouteProvider, List<MediaSink>>());
        }

        // Replace the sinks found by this provider with the new list.
        Map<MediaRouteProvider, List<MediaSink>> sinksPerProvider =
                mSinksPerSourcePerProvider.get(sourceId);
        sinksPerProvider.put(provider, sinks);

        List<MediaSink> allSinksPerSource = new ArrayList<MediaSink>();
        for (List<MediaSink> s : sinksPerProvider.values()) allSinksPerSource.addAll(s);

        mSinksPerSource.put(sourceId, allSinksPerSource);
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onSinksReceived(mNativeMediaRouterAndroidBridge,
                    ChromeMediaRouter.this, sourceId, allSinksPerSource.size());
        }
    }

    @Override
    public void onRouteCreated(String mediaRouteId, String mediaSinkId, int requestId,
            MediaRouteProvider provider, boolean wasLaunched) {
        mRouteIdsToProviders.put(mediaRouteId, provider);
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onRouteCreated(mNativeMediaRouterAndroidBridge,
                    ChromeMediaRouter.this, mediaRouteId, mediaSinkId, requestId, wasLaunched);
        }
    }

    @Override
    public void onRouteRequestError(String errorText, int requestId) {
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onRouteRequestError(
                    mNativeMediaRouterAndroidBridge, ChromeMediaRouter.this, errorText, requestId);
        }
    }

    @Override
    public void onRouteTerminated(String mediaRouteId) {
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onRouteTerminated(
                    mNativeMediaRouterAndroidBridge, ChromeMediaRouter.this, mediaRouteId);
        }
        mRouteIdsToProviders.remove(mediaRouteId);
    }

    @Override
    public void onRouteClosed(String mediaRouteId, String error) {
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onRouteClosed(
                    mNativeMediaRouterAndroidBridge, ChromeMediaRouter.this, mediaRouteId, error);
        }
        mRouteIdsToProviders.remove(mediaRouteId);
    }

    @Override
    public void onMessage(String mediaRouteId, String message) {
        if (mNativeMediaRouterAndroidBridge != 0) {
            ChromeMediaRouterJni.get().onMessage(
                    mNativeMediaRouterAndroidBridge, ChromeMediaRouter.this, mediaRouteId, message);
        }
    }

    /**
     * Initializes the media router and its providers.
     * @param nativeMediaRouterAndroidBridge the handler for the native counterpart of this instance
     * @return an initialized {@link ChromeMediaRouter} instance
     */
    @CalledByNative
    public static ChromeMediaRouter create(long nativeMediaRouterAndroidBridge) {
        ChromeMediaRouter router = new ChromeMediaRouter(nativeMediaRouterAndroidBridge);
        sRouteProviderFactory.addProviders(router);
        return router;
    }

    /**
     * Starts background monitoring for available media sinks compatible with the given
     * |sourceUrn| if the device is in a state that allows it.
     * @param sourceId a URL to use for filtering of the available media sinks
     * @return whether the monitoring started (ie. was allowed).
     */
    @CalledByNative
    public boolean startObservingMediaSinks(String sourceId) {
        Log.d(TAG, "startObservingMediaSinks: " + sourceId);
        if (SysUtils.isLowEndDevice()) return false;

        for (MediaRouteProvider provider : mRouteProviders) {
            provider.startObservingMediaSinks(sourceId);
        }

        return true;
    }

    /**
     * Stops background monitoring for available media sinks compatible with the given
     * |sourceUrn|
     * @param sourceId a URL passed to {@link #startObservingMediaSinks(String)} before.
     */
    @CalledByNative
    public void stopObservingMediaSinks(String sourceId) {
        Log.d(TAG, "stopObservingMediaSinks: " + sourceId);
        for (MediaRouteProvider provider : mRouteProviders) {
            provider.stopObservingMediaSinks(sourceId);
        }
        mSinksPerSource.remove(sourceId);
        mSinksPerSourcePerProvider.remove(sourceId);
    }

    /**
     * Returns the URN of the media sink corresponding to the given source URN
     * and an index. Essentially a way to access the corresponding {@link MediaSink}'s
     * list via JNI.
     * @param sourceUrn The URN to get the sink for.
     * @param index The index of the sink in the current sink array.
     * @return the corresponding sink URN if found or null.
     */
    @CalledByNative
    public String getSinkUrn(String sourceUrn, int index) {
        return getSink(sourceUrn, index).getUrn();
    }

    /**
     * Returns the name of the media sink corresponding to the given source URN
     * and an index. Essentially a way to access the corresponding {@link MediaSink}'s
     * list via JNI.
     * @param sourceUrn The URN to get the sink for.
     * @param index The index of the sink in the current sink array.
     * @return the corresponding sink name if found or null.
     */
    @CalledByNative
    public String getSinkName(String sourceUrn, int index) {
        return getSink(sourceUrn, index).getName();
    }

    /**
     * Initiates route creation with the given parameters. Notifies the native client of success
     * and failure.
     * @param sourceId the id of the {@link MediaSource} to route to the sink.
     * @param sinkId the id of the {@link MediaSink} to route the source to.
     * @param presentationId the id of the presentation to be used by the page.
     * @param origin the origin of the frame requesting a new route.
     * @param tabId the id of the tab the requesting frame belongs to.
     * @param isIncognito whether the route is being requested from an Incognito profile.
     * @param requestId the id of the route creation request tracked by the native side.
     */
    @CalledByNative
    public void createRoute(String sourceId, String sinkId, String presentationId, String origin,
            int tabId, boolean isIncognito, int requestId) {
        MediaRouteProvider provider = getProviderForSource(sourceId);
        if (provider == null) {
            onRouteRequestError("No provider supports createRoute with source: " + sourceId
                            + " and sink: " + sinkId,
                    requestId);
            return;
        }

        provider.createRoute(
                sourceId, sinkId, presentationId, origin, tabId, isIncognito, requestId);
    }

    /**
     * Initiates route joining with the given parameters. Notifies the native client of success
     * or failure.
     * @param sourceId the id of the {@link MediaSource} to route to the sink.
     * @param sinkId the id of the {@link MediaSink} to route the source to.
     * @param presentationId the id of the presentation to be used by the page.
     * @param origin the origin of the frame requesting a new route.
     * @param tabId the id of the tab the requesting frame belongs to.
     * @param requestId the id of the route creation request tracked by the native side.
     */
    @CalledByNative
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int requestId) {
        MediaRouteProvider provider = getProviderForSource(sourceId);
        if (provider == null) {
            onRouteRequestError("Route not found.", requestId);
            return;
        }

        provider.joinRoute(sourceId, presentationId, origin, tabId, requestId);
    }

    /**
     * Closes the route specified by the id.
     * @param routeId the id of the route to close.
     */
    @CalledByNative
    public void closeRoute(String routeId) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        if (provider == null) return;

        provider.closeRoute(routeId);
    }

    /**
     * Notifies the specified route that it's not attached to the web page anymore.
     * @param routeId the id of the route that was detached.
     */
    @CalledByNative
    public void detachRoute(String routeId) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        if (provider == null) return;

        provider.detachRoute(routeId);
        mRouteIdsToProviders.remove(routeId);
    }

    /**
     * Sends a string message to the specified route.
     * @param routeId The id of the route to send the message to.
     * @param message The message to send.
     */
    @CalledByNative
    public void sendStringMessage(String routeId, String message) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        if (provider == null) {
            return;
        }

        provider.sendStringMessage(routeId, message);
    }

    /**
     * Gets a media controller to be used by native.
     * @param routeId The route ID tied to the CastSession for which we want a media controller.
     * @return A MediaControllerBridge if it can be obtained from |routeId|, null otherwise.
     */
    @Nullable
    @CalledByNative
    public FlingingControllerBridge getFlingingControllerBridge(String routeId) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        if (provider == null) return null;

        FlingingController controller = provider.getFlingingController(routeId);
        if (controller == null) return null;

        return new FlingingControllerBridge(controller);
    }

    @VisibleForTesting
    protected ChromeMediaRouter(long nativeMediaRouterAndroidBridge) {
        mNativeMediaRouterAndroidBridge = nativeMediaRouterAndroidBridge;
    }

    /**
     * Called when the native object is being destroyed.
     */
    @CalledByNative
    public void teardown() {
        // The native object has been destroyed.
        mNativeMediaRouterAndroidBridge = 0;
    }

    private MediaSink getSink(String sourceId, int index) {
        assert mSinksPerSource.containsKey(sourceId);
        return mSinksPerSource.get(sourceId).get(index);
    }

    private MediaRouteProvider getProviderForSource(String sourceId) {
        for (MediaRouteProvider provider : mRouteProviders) {
            if (provider.supportsSource(sourceId)) return provider;
        }
        return null;
    }

    @NativeMethods
    interface Natives {
        void onSinksReceived(long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller,
                String sourceUrn, int count);
        void onRouteCreated(long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller,
                String mediaRouteId, String mediaSinkId, int createRouteRequestId,
                boolean wasLaunched);
        void onRouteRequestError(long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller,
                String errorText, int createRouteRequestId);
        void onRouteTerminated(
                long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller, String mediaRouteId);
        void onRouteClosed(long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller,
                String mediaRouteId, String message);
        void onMessage(long nativeMediaRouterAndroidBridge, ChromeMediaRouter caller,
                String mediaRouteId, String message);
    }
}
