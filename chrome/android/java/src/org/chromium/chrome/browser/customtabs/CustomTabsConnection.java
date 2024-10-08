// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.ExperimentalMinimizationCallback;
import androidx.browser.customtabs.PostMessageServiceConnection;
import androidx.browser.customtabs.PrefetchOptions;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.customtabs.ClientManager.CalledWarmup;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.variations.SyntheticTrialAnnotationMode;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.WindowAndroid;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/**
 * Implementation of the ICustomTabsService interface.
 *
 * <p>Note: This class is meant to be package private, and is public to be accessible from {@link
 * ChromeApplicationImpl}.
 */
@JNINamespace("customtabs")
@MockedInTests
public class CustomTabsConnection {
    private static final String TAG = "ChromeConnection";
    private static final String LOG_SERVICE_REQUESTS = "custom-tabs-log-service-requests";

    // Callback names for |extraCallback()|.
    @VisibleForTesting static final String PAGE_LOAD_METRICS_CALLBACK = "NavigationMetrics";
    static final String BOTTOM_BAR_SCROLL_STATE_CALLBACK = "onBottomBarScrollStateChanged";
    @VisibleForTesting static final String OPEN_IN_BROWSER_CALLBACK = "onOpenInBrowser";
    @VisibleForTesting static final String ON_WARMUP_COMPLETED = "onWarmupCompleted";

    @VisibleForTesting
    static final String ON_DETACHED_REQUEST_REQUESTED = "onDetachedRequestRequested";

    @VisibleForTesting
    static final String ON_DETACHED_REQUEST_COMPLETED = "onDetachedRequestCompleted";

    // Constants for sending connection characteristics.
    public static final String DATA_REDUCTION_ENABLED = "dataReductionEnabled";

    // "/bg_non_interactive" is from L MR1, "/apps/bg_non_interactive" before,
    // and "background" from O.
    @VisibleForTesting
    static final Set<String> BACKGROUND_GROUPS =
            new HashSet<>(
                    Arrays.asList(
                            "/bg_non_interactive", "/apps/bg_non_interactive", "/background"));

    // TODO(lizeb): Move to the support library.
    @VisibleForTesting
    static final String REDIRECT_ENDPOINT_KEY = "androidx.browser.REDIRECT_ENDPOINT";

    @VisibleForTesting
    static final String PARALLEL_REQUEST_REFERRER_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_REFERRER";

    static final String PARALLEL_REQUEST_REFERRER_POLICY_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_REFERRER_POLICY";

    @VisibleForTesting
    static final String PARALLEL_REQUEST_URL_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_URL";

    static final String RESOURCE_PREFETCH_URL_LIST_KEY =
            "androidx.browser.RESOURCE_PREFETCH_URL_LIST";

    private static final String ON_RESIZED_CALLBACK = "onResized";
    private static final String ON_RESIZED_SIZE_EXTRA = "size";

    @VisibleForTesting
    static final String IS_EPHEMERAL_BROWSING_SUPPORTED = "isEphemeralBrowsingSupported";

    @VisibleForTesting
    static final String EPHEMERAL_BROWSING_SUPPORTED_KEY = "ephemeralBrowsingSupported";

    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_CALLBACK = "onActivityLayout";
    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_LEFT_EXTRA = "left";
    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_TOP_EXTRA = "top";
    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_RIGHT_EXTRA = "right";
    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA = "bottom";
    @VisibleForTesting static final String ON_ACTIVITY_LAYOUT_STATE_EXTRA = "state";

    @IntDef({
        ParallelRequestStatus.NO_REQUEST,
        ParallelRequestStatus.SUCCESS,
        ParallelRequestStatus.FAILURE_NOT_INITIALIZED,
        ParallelRequestStatus.FAILURE_NOT_AUTHORIZED,
        ParallelRequestStatus.FAILURE_INVALID_URL,
        ParallelRequestStatus.FAILURE_INVALID_REFERRER,
        ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ParallelRequestStatus {
        // Values should start from 0 and can't have gaps (they're used for indexing
        // PARALLEL_REQUEST_MESSAGES).
        @VisibleForTesting int NO_REQUEST = 0;
        @VisibleForTesting int SUCCESS = 1;
        @VisibleForTesting int FAILURE_NOT_INITIALIZED = 2;
        @VisibleForTesting int FAILURE_NOT_AUTHORIZED = 3;
        @VisibleForTesting int FAILURE_INVALID_URL = 4;
        @VisibleForTesting int FAILURE_INVALID_REFERRER = 5;
        @VisibleForTesting int FAILURE_INVALID_REFERRER_FOR_SESSION = 6;
        int NUM_ENTRIES = 7;
    }

    private static final String[] PARALLEL_REQUEST_MESSAGES = {
        "No request",
        "Success",
        "Chrome not initialized",
        "Not authorized",
        "Invalid URL",
        "Invalid referrer",
        "Invalid referrer for session"
    };

    private static final String SYNTHETIC_FIELDTRIAL_CCT_EXPERIMENT_OVERRIDE =
            "CCT_EXPERIMENT_OVERRIDE";
    private static CustomTabsConnection sInstance;
    private @Nullable String mTrustedPublisherUrlPackage;

    private final HiddenTabHolder mHiddenTabHolder = new HiddenTabHolder();
    protected final SessionDataHolder mSessionDataHolder;

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    final ClientManager mClientManager;

    protected final boolean mLogRequests;
    private final AtomicBoolean mWarmupHasBeenCalled = new AtomicBoolean();
    private final AtomicBoolean mWarmupHasBeenFinished = new AtomicBoolean();

    @Nullable private Callback<CustomTabsSessionToken> mDisconnectCallback;

    private volatile ChainedTasks mWarmupTasks;

    // Caches the previous height reported via |onResized|. Used for extraCallback
    // |ON_RESIZED_CALLLBACK| which cares about height only.
    private int mPrevHeight;

    /** Whether Dynamic Features are enabled. CCT Intents can override the feature set. */
    private boolean mIsDynamicIntentFeatureOverridesEnabled =
            ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled();

    @Nullable private List<String> mDynamicEnabledFeatures;
    @Nullable private List<String> mDynamicDisabledFeatures;

    // Async tab prewarming can cause flakiness in tests when it runs after test shutdown and
    // triggers LifetimeAsserts.
    @VisibleForTesting public static boolean sSkipTabPrewarmingForTesting;

    /**
     * <strong>DO NOT CALL</strong> Public to be instanciable from {@link ChromeApplicationImpl}.
     * This is however intended to be private.
     */
    public CustomTabsConnection() {
        super();
        mClientManager = new ClientManager();
        mLogRequests = CommandLine.getInstance().hasSwitch(LOG_SERVICE_REQUESTS);
        mSessionDataHolder = ChromeApplicationImpl.getComponent().resolveSessionDataHolder();
    }

    /**
     * @return The unique instance of ChromeCustomTabsConnection.
     */
    public static CustomTabsConnection getInstance() {
        if (sInstance == null) {
            CustomTabsConnection impl = ServiceLoaderUtil.maybeCreate(CustomTabsConnection.class);
            if (impl == null) {
                impl = new CustomTabsConnection();
            }
            sInstance = impl;
        }

        return sInstance;
    }

    private static boolean hasInstance() {
        return sInstance != null;
    }

    /**
     * If service requests logging is enabled, logs that a call was made.
     *
     * No rate-limiting, can be spammy if the app is misbehaved.
     *
     * @param name Call name to log.
     * @param result The return value for the logged call.
     */
    void logCall(String name, Object result) {
        if (!mLogRequests) return;
        Log.w(TAG, "%s = %b, Calling UID = %d", name, result, Binder.getCallingUid());
    }

    /**
     * If service requests logging is enabled, logs a callback.
     *
     * No rate-limiting, can be spammy if the app is misbehaved.
     *
     * @param name Callback name to log.
     * @param args arguments of the callback.
     */
    void logCallback(String name, Object args) {
        if (!mLogRequests) return;
        Log.w(TAG, "%s args = %s", name, args);
    }

    /**
     * Converts a Bundle to JSON.
     *
     * The conversion is limited to Bundles not containing any array, and some elements are
     * converted into strings.
     *
     * @param bundle a Bundle to convert.
     * @return A JSON object, empty object if the parameter is null.
     */
    protected static JSONObject bundleToJson(Bundle bundle) {
        JSONObject json = new JSONObject();
        if (bundle == null) return json;
        for (String key : bundle.keySet()) {
            Object o = bundle.get(key);
            try {
                if (o instanceof Bundle b) {
                    json.put(key, bundleToJson(b));
                } else if (o instanceof Integer || o instanceof Long || o instanceof Boolean) {
                    json.put(key, o);
                } else if (o == null) {
                    json.put(key, JSONObject.NULL);
                } else {
                    json.put(key, o.toString());
                }
            } catch (JSONException e) {
                // Ok, only used for logging.
            }
        }
        return json;
    }

    /*
     * Logging for page load metrics callback, if service has enabled logging.
     *
     * No rate-limiting, can be spammy if the app is misbehaved.
     *
     * @param args arguments of the callback.
     */
    void logPageLoadMetricsCallback(Bundle args) {
        if (!mLogRequests) return; // Don't build args if not necessary.
        logCallback(
                "extraCallback(" + PAGE_LOAD_METRICS_CALLBACK + ")", bundleToJson(args).toString());
    }

    /** Sets a callback to be triggered when a service connection is terminated. */
    public void setDisconnectCallback(@Nullable Callback<CustomTabsSessionToken> callback) {
        mDisconnectCallback = callback;
    }

    public boolean newSession(CustomTabsSessionToken session) {
        boolean success = newSessionInternal(session);
        logCall("newSession()", success);
        return success;
    }

    private boolean newSessionInternal(CustomTabsSessionToken session) {
        if (session == null) return false;
        ClientManager.DisconnectCallback onDisconnect =
                new ClientManager.DisconnectCallback() {
                    @Override
                    public void run(CustomTabsSessionToken session) {
                        cancelSpeculation(session);
                        if (mDisconnectCallback != null) {
                            mDisconnectCallback.onResult(session);
                        }

                        // TODO(pshmakov): invert this dependency by moving event dispatching to a
                        // separate class.
                        ChromeApplicationImpl.getComponent()
                                .resolveCustomTabsFileProcessor()
                                .onSessionDisconnected(session);
                    }
                };

        // TODO(peconn): Make this not an anonymous class once PostMessageServiceConnection is made
        // non-abstract in AndroidX.
        PostMessageServiceConnection serviceConnection =
                new PostMessageServiceConnection(session) {};
        PostMessageHandler handler = new PostMessageHandler(serviceConnection);
        var engagementSignalsHandler = new EngagementSignalsHandler(this, session);
        return mClientManager.newSession(
                session,
                Binder.getCallingUid(),
                onDisconnect,
                handler,
                serviceConnection,
                engagementSignalsHandler);
    }

    /**
     * Overrides the given session's packageName if it is generated by Chrome. To be used for
     * testing only. To be called before the session given is associated with a tab.
     * @param session The session for which the package name should be overridden.
     * @param packageName The new package name to set.
     */
    public void overridePackageNameForSessionForTesting(
            CustomTabsSessionToken session, String packageName) {
        String originalPackage = getClientPackageNameForSession(session);
        String selfPackage = ContextUtils.getApplicationContext().getPackageName();
        if (TextUtils.isEmpty(originalPackage) || !selfPackage.equals(originalPackage)) return;
        mClientManager.overridePackageNameForSessionForTesting(session, packageName); // IN-TEST
    }

    /** Warmup activities that should only happen once. */
    private static void initializeBrowser(final Context context) {
        ThreadUtils.assertOnUiThread();
        ChromeBrowserInitializer.getInstance().handleSynchronousStartupWithGpuWarmUp();
        ChildProcessLauncherHelper.warmUpOnAnyThread(context, true);
    }

    public boolean warmup(long flags) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.warmup")) {
            boolean success = warmupInternal(true, null);
            logCall("warmup()", success);
            return success;
        }
    }

    /**
     * @return Whether {@link CustomTabsConnection#warmup(long)} has been called.
     */
    public boolean hasWarmUpBeenFinished() {
        return mWarmupHasBeenFinished.get();
    }

    /**
     * Starts as much as possible in anticipation of a future navigation.
     *
     * @param mayCreateSpareWebContents true if warmup() can create a spare renderer.
     * @param internalCallback callback to be called after all processes are finished.
     * @return true for success.
     */
    private boolean warmupInternal(
            final boolean mayCreateSpareWebContents, Runnable internalCallback) {
        // Here and in mayLaunchUrl(), don't do expensive work for background applications.
        if (!isCallerForegroundOrSelf()) return false;
        int uid = Binder.getCallingUid();
        mClientManager.recordUidHasCalledWarmup(uid);
        final boolean initialized = !mWarmupHasBeenCalled.compareAndSet(false, true);

        // The call is non-blocking and this must execute on the UI thread, post chained tasks.
        ChainedTasks tasks = new ChainedTasks();

        // Ordering of actions here:
        // 1. Initializing the browser needs to be done once, and first.
        // 2. Creating a spare renderer takes time, in other threads and processes, so start it
        //    sooner rather than later. Can be done several times.
        // 3. UI inflation has to be done for any new activity.
        // 4. Initializing the LoadingPredictor is done once, and triggers work on other threads,
        //    start it early.
        // 5. RequestThrottler first access has to be done only once.

        // (1)
        if (!initialized) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        try (TraceEvent e =
                                TraceEvent.scoped("CustomTabsConnection.initializeBrowser()")) {
                            initializeBrowser(ContextUtils.getApplicationContext());
                            ProcessInitializationHandler.getInstance().initNetworkChangeNotifier();
                            mWarmupHasBeenFinished.set(true);
                        }
                    });
        }

        // (2)
        if (mayCreateSpareWebContents && !mHiddenTabHolder.hasHiddenTab()) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Temporary fix for https://crbug.com/797832.
                        // TODO(lizeb): Properly fix instead of papering over the bug, this code
                        // should not be scheduled unless startup is done. See
                        // https://crbug.com/797832.
                        if (!BrowserStartupController.getInstance().isFullBrowserStarted()) return;
                        try (TraceEvent e = TraceEvent.scoped("CreateSpareWebContents")) {
                            createSpareWebContents(ProfileManager.getLastUsedRegularProfile());
                        }
                    });
        }

        // (3)
        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try (TraceEvent e = TraceEvent.scoped("InitializeViewHierarchy")) {
                        WarmupManager.getInstance()
                                .initializeViewHierarchy(
                                        ContextUtils.getApplicationContext(),
                                        R.layout.custom_tabs_control_container,
                                        R.layout.custom_tabs_toolbar);
                    }
                });

        if (!initialized) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        try (TraceEvent e =
                                TraceEvent.scoped("WarmupInternalFinishInitialization")) {
                            // (4)
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            WarmupManager.startPreconnectPredictorInitialization(profile);

                            // (5) The throttling database uses shared preferences, that can cause
                            // a StrictMode violation on the first access. Make sure that this
                            // access is not in mayLauchUrl.
                            RequestThrottler.loadInBackground();
                        }
                    });
        }

        tasks.add(TaskTraits.UI_DEFAULT, () -> notifyWarmupIsDone(uid, internalCallback));
        tasks.start(false);
        mWarmupTasks = tasks;
        return true;
    }

    /** @return the URL or null if it's invalid. */
    private static boolean isValid(Uri uri) {
        if (uri == null) return false;
        // Don't do anything for unknown schemes. Not having a scheme is allowed, as we allow
        // "www.example.com".
        String scheme = uri.normalizeScheme().getScheme();
        boolean allowedScheme =
                scheme == null
                        || scheme.equals(UrlConstants.HTTP_SCHEME)
                        || scheme.equals(UrlConstants.HTTPS_SCHEME);
        return allowedScheme;
    }

    /**
     * High confidence mayLaunchUrl() call, that is:
     * - Tries to speculate if possible.
     * - An empty URL cancels the current prerender if any.
     * - Start a spare renderer if necessary.
     */
    private void highConfidenceMayLaunchUrl(
            CustomTabsSessionToken session,
            int uid,
            String url,
            Bundle extras,
            List<Bundle> otherLikelyBundles) {
        ThreadUtils.assertOnUiThread();
        if (TextUtils.isEmpty(url)) {
            cancelSpeculation(session);
            return;
        }

        if (maySpeculate(session)) {
            // `IntentHandler.hasAnyIncognitoExtra` check:
            // Hidden tabs are created always with regular profile, so we need to block hidden tab
            // creation in incognito mode not to have inconsistent modes between tab model and
            // hidden tab. (crbug.com/1190971)
            // The incognito check is already performed in the entrypoint
            // `mayLaunchUrlInternal`,
            // but also performed here to be safe against future callers.
            // Read the discussion at
            // https://chromium-review.googlesource.com/c/chromium/src/+/5004377/comment/02cf16f4_82578ace/
            boolean canUseHiddenTab =
                    mClientManager.getCanUseHiddenTab(session)
                            && !IntentHandler.hasAnyIncognitoExtra(extras);

            boolean useSeparateStoragePartitionForExperiment =
                    ChromeFeatureList.isEnabled(
                            ChromeFeatureList.MAYLAUNCHURL_USES_SEPARATE_STORAGE_PARTITION);
            startSpeculation(
                    session,
                    url,
                    canUseHiddenTab,
                    extras,
                    uid,
                    useSeparateStoragePartitionForExperiment);
        }
        preconnectUrls(otherLikelyBundles);
    }

    /**
     * Low confidence mayLaunchUrl() call, that is:
     * - Preconnects to the ordered list of URLs.
     * - Makes sure that there is a spare renderer.
     */
    @VisibleForTesting
    boolean lowConfidenceMayLaunchUrl(List<Bundle> likelyBundles) {
        ThreadUtils.assertOnUiThread();
        if (!preconnectUrls(likelyBundles)) return false;
        createSpareWebContents(ProfileManager.getLastUsedRegularProfile());
        return true;
    }

    public Tab getHiddenTabForTesting() {
        return mHiddenTabHolder != null ? mHiddenTabHolder.getHiddenTabForTesting() : null;
    }

    private boolean preconnectUrls(List<Bundle> likelyBundles) {
        boolean atLeastOneUrl = false;
        if (likelyBundles == null) return false;
        WarmupManager warmupManager = WarmupManager.getInstance();
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        for (Bundle bundle : likelyBundles) {
            Uri uri;
            try {
                uri = IntentUtils.safeGetParcelable(bundle, CustomTabsService.KEY_URL);
            } catch (ClassCastException e) {
                continue;
            }
            if (isValid(uri)) {
                warmupManager.maybePreconnectUrlAndSubResources(profile, uri.toString());
                atLeastOneUrl = true;
            }
        }
        return atLeastOneUrl;
    }

    public boolean mayLaunchUrl(
            CustomTabsSessionToken session,
            Uri url,
            Bundle extras,
            List<Bundle> otherLikelyBundles) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.mayLaunchUrl")) {
            boolean success = mayLaunchUrlInternal(session, url, extras, otherLikelyBundles);
            logCall("mayLaunchUrl(" + url + ")", success);
            return success;
        }
    }

    private boolean mayLaunchUrlInternal(
            final CustomTabsSessionToken session,
            final Uri url,
            final Bundle extras,
            final List<Bundle> otherLikelyBundles) {
        // mayLaunchUrl should not be executed for Incognito CCT since all setup is created with
        // regular profile. If we need to enable mayLaunchUrl for off-the-record profiles, we need
        // to update the profile used. Please see crbug.com/1106757.
        if (IntentHandler.hasAnyIncognitoExtra(extras)) return false;

        final boolean lowConfidence =
                (url == null || TextUtils.isEmpty(url.toString())) && otherLikelyBundles != null;
        final String urlString = isValid(url) ? url.toString() : null;
        if (url != null && urlString == null && !lowConfidence) return false;

        final int uid = Binder.getCallingUid();

        // Things below need the browser process to be initialized.

        // Forbids warmup() from creating a spare renderer, as prerendering wouldn't reuse
        // it. Checking whether prerendering is enabled requires the native library to be loaded,
        // which is not necessarily the case yet.
        if (!warmupInternal(false, null)) return false; // Also does the foreground check.

        if (!mClientManager.updateStatsAndReturnWhetherAllowed(
                session, uid, urlString, otherLikelyBundles != null)) {
            return false;
        }

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    doMayLaunchUrlOnUiThread(
                            lowConfidence,
                            session,
                            uid,
                            urlString,
                            extras,
                            otherLikelyBundles,
                            true);
                });
        return true;
    }

    @androidx.browser.customtabs.ExperimentalPrefetch
    public void prefetch(
            CustomTabsSessionToken session, List<Uri> urls, @Nullable PrefetchOptions options) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.prefetch")) {
            if (!ChromeFeatureList.sPrefetchBrowserInitiatedTriggers.isEnabled()
                    || !ChromeFeatureList.sCctNavigationalPrefetch.isEnabled()) {
                Log.w(
                        TAG,
                        "Prefetch failed because PrefetchBrowserInitiatedTriggers and/or"
                                + " CCTNavigationalPrefetch is not enabled.");
                return;
            }
            prefetchInternal(session, urls, options);
        }
    }

    @androidx.browser.customtabs.ExperimentalPrefetch
    private void prefetchInternal(
            CustomTabsSessionToken session, List<Uri> urls, PrefetchOptions options) {
        boolean usePrefetchProxy = options.requiresAnonymousIpWhenCrossOrigin;
        Origin sourceOrigin =
                options.sourceOrigin != null
                        ? Origin.create(options.sourceOrigin.toString())
                        : null;

        // We should call
        // (1) warmupInternal to initialize browser and prepare spare WebContents for (2), (3)
        // (2) validateSourceOriginOfPrefetch to register source origin of prefetch to
        //     OriginVerifier
        // (3) startPrefetchFromCCT
        // sequentially.

        // (3)
        Runnable startPrefetch =
                () -> {
                    String verifiedSourceOrigin =
                            isValidForPrefetchSourceOrigin(session, sourceOrigin)
                                    ? sourceOrigin.toString()
                                    : null;
                    for (Uri url : urls) {
                        String urlString = isValid(url) ? url.toString() : null;
                        if (urlString == null) continue;
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    WarmupManager.getInstance()
                                            .startPrefetchFromCCT(
                                                    urlString,
                                                    usePrefetchProxy,
                                                    verifiedSourceOrigin);
                                });
                    }
                };

        // (2)
        Runnable validateOrigin =
                () -> {
                    if (sourceOrigin != null) {
                        mClientManager.validateSourceOriginOfPrefetch(
                                session, sourceOrigin, startPrefetch);
                    } else {
                        startPrefetch.run();
                    }
                };

        // (1)
        warmupInternal(true, validateOrigin);
    }

    @VisibleForTesting
    @androidx.browser.customtabs.ExperimentalPrefetch
    boolean isValidForPrefetchSourceOrigin(
            CustomTabsSessionToken session, @Nullable Origin origin) {
        return origin != null && mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    private void enableExperimentIdsIfNecessary(Bundle extras) {
        ThreadUtils.assertOnUiThread();
        if (extras == null) return;
        int[] experimentIds =
                IntentUtils.safeGetIntArray(extras, CustomTabIntentDataProvider.EXPERIMENT_IDS);
        if (experimentIds == null) return;
        // When ids are set through cct, they should not override existing ids.
        boolean override = false;
        UmaSessionStats.registerExternalExperiment(experimentIds, override);
    }

    private void doMayLaunchUrlOnUiThread(
            final boolean lowConfidence,
            final CustomTabsSessionToken session,
            final int uid,
            final String urlString,
            final Bundle extras,
            final List<Bundle> otherLikelyBundles,
            boolean retryIfNotLoaded) {
        ThreadUtils.assertOnUiThread();
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.mayLaunchUrlOnUiThread")) {
            // doMayLaunchUrlInternal() is always called once the native level initialization is
            // done, at least the initial profile load. However, at that stage the startup callback
            // may not have run, which causes ProfileManager.getLastUsedRegularProfile() to throw an
            // exception. But the tasks have been posted by then, so reschedule ourselves, only
            // once.
            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                if (retryIfNotLoaded) {
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                doMayLaunchUrlOnUiThread(
                                        lowConfidence,
                                        session,
                                        uid,
                                        urlString,
                                        extras,
                                        otherLikelyBundles,
                                        false);
                            });
                }
                return;
            }

            enableExperimentIdsIfNecessary(extras);

            if (lowConfidence) {
                lowConfidenceMayLaunchUrl(otherLikelyBundles);
            } else {
                highConfidenceMayLaunchUrl(session, uid, urlString, extras, otherLikelyBundles);
            }
        }
    }

    /**
     * Sends a command that isn't part of the API yet.
     *
     * @param commandName Name of the extra command to execute.
     * @param args Arguments for the command.
     * @return The result {@link Bundle}, or null.
     */
    public @Nullable Bundle extraCommand(String commandName, Bundle args) {
        if (commandName.equals(IS_EPHEMERAL_BROWSING_SUPPORTED)) {
            var bundle = new Bundle();
            bundle.putBoolean(
                    EPHEMERAL_BROWSING_SUPPORTED_KEY,
                    ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_EPHEMERAL_MODE));
            return bundle;
        }
        return null;
    }

    public boolean updateVisuals(final CustomTabsSessionToken session, Bundle bundle) {
        if (mLogRequests) Log.w(TAG, "updateVisuals: %s", bundleToJson(bundle));
        SessionHandler handler = mSessionDataHolder.getActiveHandler(session);
        if (handler == null) return false;

        final Bundle actionButtonBundle =
                IntentUtils.safeGetBundle(bundle, CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE);
        boolean result = true;
        List<Integer> ids = new ArrayList<>();
        List<String> descriptions = new ArrayList<>();
        List<Bitmap> icons = new ArrayList<>();
        if (actionButtonBundle != null) {
            int id =
                    IntentUtils.safeGetInt(
                            actionButtonBundle,
                            CustomTabsIntent.KEY_ID,
                            CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);
            Bitmap bitmap = CustomButtonParamsImpl.parseBitmapFromBundle(actionButtonBundle);
            String description =
                    CustomButtonParamsImpl.parseDescriptionFromBundle(actionButtonBundle);
            if (bitmap != null && description != null) {
                ids.add(id);
                descriptions.add(description);
                icons.add(bitmap);
            }
        }

        List<Bundle> bundleList =
                IntentUtils.safeGetParcelableArrayList(
                        bundle, CustomTabsIntent.EXTRA_TOOLBAR_ITEMS);
        if (bundleList != null) {
            for (Bundle toolbarItemBundle : bundleList) {
                int id =
                        IntentUtils.safeGetInt(
                                toolbarItemBundle,
                                CustomTabsIntent.KEY_ID,
                                CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);
                if (ids.contains(id)) continue;

                Bitmap bitmap = CustomButtonParamsImpl.parseBitmapFromBundle(toolbarItemBundle);
                if (bitmap == null) continue;

                String description =
                        CustomButtonParamsImpl.parseDescriptionFromBundle(toolbarItemBundle);
                if (description == null) continue;

                ids.add(id);
                descriptions.add(description);
                icons.add(bitmap);
            }
        }

        if (!ids.isEmpty()) {
            result &=
                    PostTask.runSynchronously(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                boolean res = true;
                                for (int i = 0; i < ids.size(); i++) {
                                    res &=
                                            handler.updateCustomButton(
                                                    ids.get(i), icons.get(i), descriptions.get(i));
                                }
                                return res;
                            });
        }

        if (bundle.containsKey(CustomTabsIntent.EXTRA_REMOTEVIEWS)) {
            final RemoteViews remoteViews =
                    IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.EXTRA_REMOTEVIEWS);
            final int[] clickableIDs =
                    IntentUtils.safeGetIntArray(
                            bundle, CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS);
            final PendingIntent pendingIntent =
                    IntentUtils.safeGetParcelable(
                            bundle, CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT);
            result &=
                    PostTask.runSynchronously(
                            TaskTraits.UI_DEFAULT,
                            () ->
                                    handler.updateRemoteViews(
                                            remoteViews, clickableIDs, pendingIntent));
        }

        PendingIntent pendingIntent = getSecondarySwipeToolbarSwipeUpGesture(bundle);
        if (pendingIntent != null) {
            result &=
                    PostTask.runSynchronously(
                            TaskTraits.UI_DEFAULT,
                            () ->
                                    handler.updateSecondaryToolbarSwipeUpPendingIntent(
                                            pendingIntent));
        }

        logCall("updateVisuals()", result);
        return result;
    }

    private static PendingIntent getSecondarySwipeToolbarSwipeUpGesture(Bundle bundle) {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelable(
                        bundle, CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_GESTURE);
        if (pendingIntent == null) {
            pendingIntent =
                    IntentUtils.safeGetParcelable(
                            bundle,
                            CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION);
        }
        return pendingIntent;
    }

    public boolean requestPostMessageChannel(
            CustomTabsSessionToken session,
            Origin postMessageSourceOrigin,
            @Nullable Origin postMessageTargetOrigin) {
        boolean success =
                requestPostMessageChannelInternal(
                        session, postMessageSourceOrigin, postMessageTargetOrigin);
        logCall(
                "requestPostMessageChannel() with origin "
                        + (postMessageSourceOrigin != null
                                ? postMessageSourceOrigin.toString()
                                : ""),
                success);
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.PostMessage.RequestPostMessageChannel", success);
        return success;
    }

    private boolean requestPostMessageChannelInternal(
            final CustomTabsSessionToken session,
            final Origin postMessageOrigin,
            @Nullable Origin postMessageTargetOrigin) {
        if (!mWarmupHasBeenCalled.get()) return false;
        if (!isCallerForegroundOrSelf() && !mSessionDataHolder.isActiveSession(session)) {
            return false;
        }
        if (!mClientManager.bindToPostMessageServiceForSession(session)) return false;

        final int uid = Binder.getCallingUid();
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Attempt to verify origin synchronously. If successful directly initialize
                    // postMessage channel for session.
                    Uri verifiedOrigin = verifyOriginForSession(session, uid, postMessageOrigin);
                    if (verifiedOrigin == null) {
                        mClientManager.verifyAndInitializeWithPostMessageOriginForSession(
                                session,
                                postMessageOrigin,
                                postMessageTargetOrigin,
                                CustomTabsService.RELATION_USE_AS_ORIGIN);
                    } else {
                        mClientManager.initializeWithPostMessageOriginForSession(
                                session,
                                verifiedOrigin,
                                postMessageTargetOrigin != null
                                        ? postMessageTargetOrigin.uri()
                                        : null);
                    }
                });
        return true;
    }

    /**
     * Acquire the origin for the client that owns the given session.
     * @param session The session to use for getting client information.
     * @param clientUid The UID for the client controlling the session.
     * @param origin The origin that is suggested by the client. The validated origin may be this or
     *               a derivative of this.
     * @return The validated origin {@link Uri} for the given session's client.
     */
    protected Uri verifyOriginForSession(
            CustomTabsSessionToken session, int clientUid, Origin origin) {
        if (clientUid == Process.myUid()) return Uri.EMPTY;
        return null;
    }

    /**
     * Returns whether an intent is first-party with respect to its session, that is if the
     * application linked to the session has a relation with the provided origin.
     *
     * @param intent The intent to verify.
     */
    public boolean isFirstPartyOriginForIntent(Intent intent) {
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (session == null) return false;

        Origin origin = Origin.create(intent.getData());
        if (origin == null) return false;

        return mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    public int postMessage(CustomTabsSessionToken session, String message, Bundle extras) {
        int result;
        if (!mWarmupHasBeenCalled.get()) result = CustomTabsService.RESULT_FAILURE_DISALLOWED;
        if (!isCallerForegroundOrSelf() && !mSessionDataHolder.isActiveSession(session)) {
            result = CustomTabsService.RESULT_FAILURE_DISALLOWED;
        }
        // If called before a validatePostMessageOrigin, the post message origin will be invalid and
        // will return a failure result here.
        result = mClientManager.postMessage(session, message);
        logCall("postMessage", result);
        return result;
    }

    public boolean validateRelationship(
            CustomTabsSessionToken sessionToken, int relation, Origin origin, Bundle extras) {
        // Essential parts of the verification will depend on native code and will be run sync on UI
        // thread. Make sure the client has called warmup() beforehand.
        if (!mWarmupHasBeenCalled.get()) {
            Log.d(TAG, "Verification failed due to warmup not having been previously called.");
            mClientManager
                    .getCallbackForSession(sessionToken)
                    .onRelationshipValidationResult(
                            relation, Uri.parse(origin.toString()), false, null);
            return false;
        }
        return mClientManager.validateRelationship(sessionToken, relation, origin, extras);
    }

    /**
     * See
     * {@link ClientManager#resetPostMessageHandlerForSession(CustomTabsSessionToken, WebContents)}.
     */
    public void resetPostMessageHandlerForSession(
            CustomTabsSessionToken session, WebContents webContents) {
        mClientManager.resetPostMessageHandlerForSession(session, webContents);
    }

    /**
     * Registers a launch of a |url| for a given |session|.
     *
     * This is used for accounting.
     */
    void registerLaunch(CustomTabsSessionToken session, String url) {
        mClientManager.registerLaunch(session, url);
    }

    public @Nullable String getSpeculatedUrl(CustomTabsSessionToken session) {
        return mHiddenTabHolder.getSpeculatedUrl(session);
    }

    /**
     * Returns the preloaded {@link Tab} if it matches the given |url| and |referrer|. Null if no
     * such {@link Tab}. If a {@link Tab} is preloaded but it does not match, it is discarded.
     *
     * @param session The Binder object identifying a session.
     * @param url The URL the tab is for.
     * @param referrer The referrer to use for |url|.
     * @return The hidden tab, or null.
     */
    public @Nullable Tab takeHiddenTab(
            @Nullable CustomTabsSessionToken session, String url, @Nullable String referrer) {
        return mHiddenTabHolder.takeHiddenTab(
                session, mClientManager.getIgnoreFragmentsForSession(session), url, referrer);
    }

    /**
     * Called when an intent is handled by either an existing or a new CustomTabActivity.
     *
     * @param session Session extracted from the intent.
     * @param intent incoming intent.
     */
    public void onHandledIntent(CustomTabsSessionToken session, Intent intent) {
        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) {
            return;
        }
        if (mLogRequests) {
            Log.w(
                    TAG,
                    "onHandledIntent, URL: %s, extras: %s",
                    url,
                    bundleToJson(intent.getExtras()));
        }

        // If we still have pending warmup tasks, don't continue as they would only delay intent
        // processing from now on.
        if (mWarmupTasks != null) mWarmupTasks.cancel();

        maybePreconnectToRedirectEndpoint(session, url, intent);
        ChromeBrowserInitializer.getInstance()
                .runNowOrAfterFullBrowserStarted(() -> handleParallelRequest(session, intent));
        maybePrefetchResources(session, intent);
    }

    /**
     * Called each time a CCT tab is created to check if a client data header was set and if so
     * forward it along to the native side.
     * @param session Session identifier.
     * @param webContents the WebContents of the new tab.
     */
    public void setClientDataHeaderForNewTab(
            CustomTabsSessionToken session, WebContents webContents) {}

    protected void setClientDataHeader(WebContents webContents, String header) {
        if (TextUtils.isEmpty(header)) return;

        CustomTabsConnectionJni.get().setClientDataHeader(webContents, header);
    }

    private void maybePreconnectToRedirectEndpoint(
            CustomTabsSessionToken session, String url, Intent intent) {
        // For the preconnection to not be a no-op, we need more than just the native library.
        if (!ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
            return;
        }

        // Conditions:
        // - There is a valid redirect endpoint.
        // - The URL's origin is first party with respect to the app.
        Uri redirectEndpoint = intent.getParcelableExtra(REDIRECT_ENDPOINT_KEY);
        if (redirectEndpoint == null || !isValid(redirectEndpoint)) return;

        Origin origin = Origin.create(url);
        if (origin == null) return;
        if (!mClientManager.isFirstPartyOriginForSession(session, origin)) return;

        WarmupManager.getInstance()
                .maybePreconnectUrlAndSubResources(
                        ProfileManager.getLastUsedRegularProfile(), redirectEndpoint.toString());
    }

    @VisibleForTesting
    @ParallelRequestStatus
    int handleParallelRequest(CustomTabsSessionToken session, Intent intent) {
        int status = maybeStartParallelRequest(session, intent);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.ParallelRequestStatusOnStart",
                status,
                ParallelRequestStatus.NUM_ENTRIES);

        if (mLogRequests) {
            Log.w(TAG, "handleParallelRequest() = " + PARALLEL_REQUEST_MESSAGES[status]);
        }

        if ((status != ParallelRequestStatus.NO_REQUEST)
                && (status != ParallelRequestStatus.FAILURE_NOT_INITIALIZED)
                && (status != ParallelRequestStatus.FAILURE_NOT_AUTHORIZED)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)) {
            Bundle args = new Bundle();
            Uri url = intent.getParcelableExtra(PARALLEL_REQUEST_URL_KEY);
            args.putParcelable("url", url);
            args.putInt("status", status);
            safeExtraCallback(session, ON_DETACHED_REQUEST_REQUESTED, args);
            if (mLogRequests) {
                logCallback(ON_DETACHED_REQUEST_REQUESTED, bundleToJson(args).toString());
            }
        }

        return status;
    }

    /**
     * Maybe starts a parallel request.
     *
     * @param session Calling context session.
     * @param intent Incoming intent with the extras.
     * @return Whether the request was started, with reason in case of failure.
     */
    private @ParallelRequestStatus int maybeStartParallelRequest(
            CustomTabsSessionToken session, Intent intent) {
        ThreadUtils.assertOnUiThread();

        if (!intent.hasExtra(PARALLEL_REQUEST_URL_KEY)) return ParallelRequestStatus.NO_REQUEST;
        if (!ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
            return ParallelRequestStatus.FAILURE_NOT_INITIALIZED;
        }
        if (!mClientManager.getAllowParallelRequestForSession(session)) {
            return ParallelRequestStatus.FAILURE_NOT_AUTHORIZED;
        }
        Uri referrer = intent.getParcelableExtra(PARALLEL_REQUEST_REFERRER_KEY);
        Uri url = intent.getParcelableExtra(PARALLEL_REQUEST_URL_KEY);
        int policy =
                intent.getIntExtra(PARALLEL_REQUEST_REFERRER_POLICY_KEY, ReferrerPolicy.DEFAULT);
        if (url == null) return ParallelRequestStatus.FAILURE_INVALID_URL;
        if (referrer == null) return ParallelRequestStatus.FAILURE_INVALID_REFERRER;
        if (policy < ReferrerPolicy.MIN_VALUE || policy > ReferrerPolicy.MAX_VALUE) {
            policy = ReferrerPolicy.DEFAULT;
        }

        if (url.toString().equals("") || !isValid(url)) {
            return ParallelRequestStatus.FAILURE_INVALID_URL;
        }
        if (!canDoParallelRequest(session, referrer)) {
            return ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
        }

        String urlString = url.toString();
        String referrerString = referrer.toString();
        String packageName = mClientManager.getClientPackageNameForSession(session);
        CustomTabsConnectionJni.get()
                .createAndStartDetachedResourceRequest(
                        ProfileManager.getLastUsedRegularProfile(),
                        session,
                        packageName,
                        urlString,
                        referrerString,
                        policy,
                        DetachedResourceRequestMotivation.PARALLEL_REQUEST);
        if (mLogRequests) {
            Log.w(TAG, "startParallelRequest(%s, %s, %d)", urlString, referrerString, policy);
        }

        return ParallelRequestStatus.SUCCESS;
    }

    /**
     * Maybe starts a resource prefetch.
     *
     * @param session Calling context session.
     * @param intent Incoming intent with the extras.
     * @return Number of prefetch requests that have been sent.
     */
    @VisibleForTesting
    int maybePrefetchResources(CustomTabsSessionToken session, Intent intent) {
        ThreadUtils.assertOnUiThread();

        if (!mClientManager.getAllowResourcePrefetchForSession(session)) return 0;

        List<Uri> resourceList = intent.getParcelableArrayListExtra(RESOURCE_PREFETCH_URL_LIST_KEY);
        Uri referrer = intent.getParcelableExtra(PARALLEL_REQUEST_REFERRER_KEY);
        int policy =
                intent.getIntExtra(PARALLEL_REQUEST_REFERRER_POLICY_KEY, ReferrerPolicy.DEFAULT);

        if (resourceList == null || referrer == null) return 0;
        if (policy < 0 || policy > ReferrerPolicy.MAX_VALUE) policy = ReferrerPolicy.DEFAULT;
        Origin origin = Origin.create(referrer);
        if (origin == null) return 0;
        if (!mClientManager.isFirstPartyOriginForSession(session, origin)) return 0;

        String referrerString = referrer.toString();
        int requestsSent = 0;
        for (Uri url : resourceList) {
            String urlString = url.toString();
            if (urlString.isEmpty() || !isValid(url)) continue;

            // Session is null because we don't need completion notifications.
            CustomTabsConnectionJni.get()
                    .createAndStartDetachedResourceRequest(
                            ProfileManager.getLastUsedRegularProfile(),
                            null,
                            null,
                            urlString,
                            referrerString,
                            policy,
                            DetachedResourceRequestMotivation.RESOURCE_PREFETCH);
            ++requestsSent;

            if (mLogRequests) {
                Log.w(TAG, "startResourcePrefetch(%s, %s, %d)", urlString, referrerString, policy);
            }
        }

        return requestsSent;
    }

    /**
     * @return Whether {@code session} can create a parallel request for a given
     * {@code referrer}.
     */
    @VisibleForTesting
    boolean canDoParallelRequest(CustomTabsSessionToken session, Uri referrer) {
        ThreadUtils.assertOnUiThread();
        Origin origin = Origin.create(referrer);
        if (origin == null) return false;
        return mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    /** @see ClientManager#shouldHideDomainForSession(CustomTabsSessionToken) */
    public boolean shouldHideDomainForSession(CustomTabsSessionToken session) {
        return mClientManager.shouldHideDomainForSession(session);
    }

    /** @see ClientManager#shouldSpeculateLoadOnCellularForSession(CustomTabsSessionToken) */
    public boolean shouldSpeculateLoadOnCellularForSession(CustomTabsSessionToken session) {
        return mClientManager.shouldSpeculateLoadOnCellularForSession(session);
    }

    /** @see ClientManager#getCanUseHiddenTab(CustomTabsSessionToken) */
    public boolean canUseHiddenTabForSession(CustomTabsSessionToken session) {
        return mClientManager.getCanUseHiddenTab(session);
    }

    /** @see ClientManager#shouldSendNavigationInfoForSession(CustomTabsSessionToken) */
    public boolean shouldSendNavigationInfoForSession(CustomTabsSessionToken session) {
        return mClientManager.shouldSendNavigationInfoForSession(session);
    }

    /** @see ClientManager#shouldSendBottomBarScrollStateForSession(CustomTabsSessionToken) */
    public boolean shouldSendBottomBarScrollStateForSession(CustomTabsSessionToken session) {
        return mClientManager.shouldSendBottomBarScrollStateForSession(session);
    }

    /** See {@link ClientManager#getClientPackageNameForSession(CustomTabsSessionToken)} */
    public String getClientPackageNameForSession(CustomTabsSessionToken session) {
        return mClientManager.getClientPackageNameForSession(session);
    }

    /**
     * @return Whether the given package name is that of a first-party application.
     */
    public boolean isFirstParty(String packageName) {
        if (packageName == null) return false;
        return ChromeApplicationImpl.getComponent()
                .resolveExternalAuthUtils()
                .isGoogleSigned(packageName);
    }

    void setIgnoreUrlFragmentsForSession(CustomTabsSessionToken session, boolean value) {
        mClientManager.setIgnoreFragmentsForSession(session, value);
    }

    @VisibleForTesting
    boolean getIgnoreUrlFragmentsForSession(CustomTabsSessionToken session) {
        return mClientManager.getIgnoreFragmentsForSession(session);
    }

    @VisibleForTesting
    void setShouldSpeculateLoadOnCellularForSession(CustomTabsSessionToken session, boolean value) {
        mClientManager.setSpeculateLoadOnCellularForSession(session, value);
    }

    @VisibleForTesting
    public void setCanUseHiddenTabForSession(CustomTabsSessionToken session, boolean value) {
        mClientManager.setCanUseHiddenTab(session, value);
    }

    /**
     * See {@link ClientManager#setSendNavigationInfoForSession(CustomTabsSessionToken, boolean)}.
     */
    void setSendNavigationInfoForSession(CustomTabsSessionToken session, boolean send) {
        mClientManager.setSendNavigationInfoForSession(session, send);
    }

    /**
     * Shows a toast about any possible sign in issues encountered during custom tab startup.
     *
     * @param session The session that the corresponding custom tab is assigned to.
     * @param intent The intent that launched the custom tab.
     * @param profileProviderSupplier The supplier of the current profile.
     */
    void showSignInToastIfNecessary(
            CustomTabsSessionToken session,
            Intent intent,
            Supplier<ProfileProvider> profileProviderSupplier) {}

    /**
     * @param session The session that the corresponding custom tab is assigned to.
     * @param intent The intent that launched the custom tab.
     * @param windowAndroid The android window.
     * @param profileProviderSupplier The supplier of the current profile.
     */
    void maybeShowAccountMismatchNotification(
            CustomTabsSessionToken session,
            Intent intent,
            WindowAndroid windowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier) {}

    /**
     * Sends a callback using {@link CustomTabsCallback} with the first run result if necessary.
     *
     * @param intentExtras The extras for the initial VIEW intent that initiated first run.
     * @param resultOK Whether first run was successful.
     */
    public void sendFirstRunCallbackIfNecessary(Bundle intentExtras, boolean resultOK) {}

    /**
     * Sends the navigation info that was captured to the client callback.
     * @param session The session to use for getting client callback.
     * @param url The current url for the tab.
     * @param title The current title for the tab.
     * @param snapshotPath Uri location for screenshot of the tab contents which is publicly
     *         available for sharing.
     */
    public void sendNavigationInfo(
            CustomTabsSessionToken session, String url, String title, Uri snapshotPath) {}

    /**
     * Called when the bottom bar for the custom tab has been hidden or shown completely by user
     * scroll.
     *
     * @param session The session that is linked with the custom tab.
     * @param hidden Whether the bottom bar is hidden or shown.
     */
    public void onBottomBarScrollStateChanged(CustomTabsSessionToken session, boolean hidden) {
        Bundle args = new Bundle();
        args.putBoolean("hidden", hidden);

        if (safeExtraCallback(session, BOTTOM_BAR_SCROLL_STATE_CALLBACK, args) && mLogRequests) {
            logCallback("extraCallback(" + BOTTOM_BAR_SCROLL_STATE_CALLBACK + ")", hidden);
        }
    }

    /** Called when a resizable Custom Tab is resized. */
    public void onResized(@Nullable CustomTabsSessionToken session, int height, int width) {
        Bundle args = new Bundle();
        if (height != mPrevHeight) {
            args.putInt(ON_RESIZED_SIZE_EXTRA, height);

            // TODO(crbug.com/40867201): Deprecate the extra callback.
            if (safeExtraCallback(session, ON_RESIZED_CALLBACK, args) && mLogRequests) {
                logCallback("extraCallback(" + ON_RESIZED_CALLBACK + ")", args);
            }
            mPrevHeight = height;
        }

        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return;
        try {
            callback.onActivityResized(height, width, args);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return;
        }
        logCallback("onActivityResized()", "(" + height + "x" + width + ")");
    }

    /** Called when a Custom Tab is unminimized. */
    @OptIn(markerClass = ExperimentalMinimizationCallback.class)
    public void onUnminimized(@Nullable CustomTabsSessionToken session) {
        Bundle args = new Bundle();

        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return;
        try {
            callback.onUnminimized(args);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return;
        }
        logCallback("onUnminimized()", args);
    }

    /** Called when a Custom Tab is minimized. */
    @OptIn(markerClass = ExperimentalMinimizationCallback.class)
    public void onMinimized(@Nullable CustomTabsSessionToken session) {
        Bundle args = new Bundle();

        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return;
        try {
            callback.onMinimized(args);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return;
        }
        logCallback("onMinimized()", args);
    }

    /**
     * Called when the Custom Tab's layout has changed.
     *
     * @param left The left coordinate of the custom tab window in pixels
     * @param top The top coordinate of the custom tab window in pixels
     * @param right The right coordinate of the custom tab window in pixels
     * @param bottom The bottom coordinate of the custom tab window in pixels
     * @param state The current layout state in which the Custom Tab is displayed.
     */
    public void onActivityLayout(
            @Nullable CustomTabsSessionToken session,
            int left,
            int top,
            int right,
            int bottom,
            @CustomTabsCallback.ActivityLayoutState int state) {
        Bundle args = new Bundle();
        args.putInt(ON_ACTIVITY_LAYOUT_LEFT_EXTRA, left);
        args.putInt(ON_ACTIVITY_LAYOUT_TOP_EXTRA, top);
        args.putInt(ON_ACTIVITY_LAYOUT_RIGHT_EXTRA, right);
        args.putInt(ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA, bottom);
        args.putInt(ON_ACTIVITY_LAYOUT_STATE_EXTRA, state);

        if (safeExtraCallback(session, ON_ACTIVITY_LAYOUT_CALLBACK, args) && mLogRequests) {
            logCallback("extraCallback(" + ON_ACTIVITY_LAYOUT_CALLBACK + ")", args);
        }

        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return;
        try {
            callback.onActivityLayout(left, top, right, bottom, state, Bundle.EMPTY);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return;
        }
    }

    /**
     * @see {@link notifyNavigationEvent(CustomTabsSessionToken, int, Optional<int>)}
     */
    public boolean notifyNavigationEvent(CustomTabsSessionToken session, int navigationEvent) {
        return notifyNavigationEvent(session, navigationEvent, Optional.empty());
    }

    /**
     * Notifies the application of a navigation event.
     *
     * <p>Delivers the {@link CustomTabsCallback#onNavigationEvent} callback to the application.
     *
     * @param session The Binder object identifying the session.
     * @param navigationEvent The navigation event code, defined in {@link CustomTabsCallback}
     * @param errorCode Network error code. Empty if there was no error or the error code is not in
     *     the list of error codes that should be passed to the embedder.
     * @return true for success.
     */
    public boolean notifyNavigationEvent(
            CustomTabsSessionToken session, int navigationEvent, Optional<Integer> errorCode) {
        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return false;
        try {
            Bundle extra = getExtrasBundleForNavigationEventForSession(session);
            if (errorCode.isPresent()) extra.putInt("navigationEventErrorCode", errorCode.get());
            callback.onNavigationEvent(navigationEvent, extra);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return false;
        }
        logCallback("onNavigationEvent()", navigationEvent);
        return true;
    }

    /** Resets dynamic experiment features that can be enabled/disabled via an Intent. */
    @VisibleForTesting
    void resetDynamicFeatures() {
        mDynamicEnabledFeatures = null;
        mDynamicDisabledFeatures = null;
    }

    /**
     * Does setup of dynamic experiment features that can be enabled/disabled via an Intent.
     *
     * @param intent The {@link Intent} that is active, to be scanned for enable/disable Extras.
     * @return Whether the setup will actually change the active feature set.
     */
    boolean setupDynamicFeatures(Intent intent) {
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (!mIsDynamicIntentFeatureOverridesEnabled
                || (!CustomTabIntentDataProvider.isTrustedCustomTab(intent, session)
                        && !CommandLine.getInstance()
                                .hasSwitch("cct-client-firstparty-override"))) {
            return false;
        }
        return setupDynamicFeaturesInternal(intent);
    }

    @VisibleForTesting
    boolean setupDynamicFeaturesInternal(Intent intent) {
        // TODO(crbug.com/40884078) Add support for separate dynamic experiments per session!
        // Early exits if any CCT client app has already set or cleared dynamic experiments.
        if (mDynamicEnabledFeatures != null || mDynamicDisabledFeatures != null) return false;

        ArrayList<String> enabledExperiments =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, CustomTabIntentDataProvider.EXPERIMENTS_ENABLE);
        ArrayList<String> disabledExperiments =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, CustomTabIntentDataProvider.EXPERIMENTS_DISABLE);
        if (!areExperimentsSupported(enabledExperiments, disabledExperiments)) return false;

        mDynamicEnabledFeatures = enabledExperiments;
        mDynamicDisabledFeatures = disabledExperiments;
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            boolean isEnabling = enabledExperiments != null;
            String groupPrefix = isEnabling ? "Enable_" : "Disable_";
            List<String> featuresUsed = isEnabling ? enabledExperiments : disabledExperiments;
            String groupName = groupPrefix + String.join("_", featuresUsed);
            UmaSessionStats.registerSyntheticFieldTrial(
                    SYNTHETIC_FIELDTRIAL_CCT_EXPERIMENT_OVERRIDE,
                    groupName,
                    SyntheticTrialAnnotationMode.CURRENT_LOG);
        } else {
            Log.w(TAG, "The Metrics Service is not available, so no synthetic field trial");
        }
        return true;
    }

    /**
     * Determines whether the given enable and disable features are currently supported.
     * @param enabledExperiments A list of Features to enable.
     * @param disabledExperiments A list of Features to disable.
     * @return Whether this set of Features is allowed to be overridden by an Intent.
     */
    @VisibleForTesting
    boolean areExperimentsSupported(
            List<String> enabledExperiments, List<String> disabledExperiments) {
        return false;
    }

    // TODO(crbug.com/40274032): Remove this and other dynamic feature related methods.
    /**
     * Determines if the given Feature is enabled after factoring in active Intent overrides.
     *
     * @see #setupDynamicFeatures
     * @param featureName The Feature to check if it's enabled.
     * @return Whether the given Feature is effectively enabled given active overrides.
     */
    public boolean isDynamicFeatureEnabled(String featureName) {
        if (mIsDynamicIntentFeatureOverridesEnabled) {
            if (mDynamicEnabledFeatures != null && mDynamicEnabledFeatures.contains(featureName)) {
                return true;
            }
            if (mDynamicDisabledFeatures != null
                    && mDynamicDisabledFeatures.contains(featureName)) {
                return false;
            }
        }
        Log.e(TAG, "Unsupported Feature!");
        return false;
    }

    @VisibleForTesting
    void setIsDynamicFeaturesEnabled(boolean isDynamicFeaturesEnabled) {
        mIsDynamicIntentFeatureOverridesEnabled = isDynamicFeaturesEnabled;
    }

    /**
     * Returns whether the given feature is enabled with Intent overrides.
     * @param featureName The feature to check.
     * @return Whether the feature is enabled with Intent overrides.
     */
    public boolean isDynamicFeatureEnabledWithOverrides(String featureName) {
        return mDynamicEnabledFeatures != null && mDynamicEnabledFeatures.contains(featureName);
    }

    /**
     * @return The {@link Bundle} to use as extra to
     *         {@link CustomTabsCallback#onNavigationEvent(int, Bundle)}
     */
    protected Bundle getExtrasBundleForNavigationEventForSession(CustomTabsSessionToken session) {
        // SystemClock.uptimeMillis() is used here as it (as of June 2017) uses the same system call
        // as all the native side of Chrome, and this is the same clock used for page load metrics.
        Bundle extras = new Bundle();
        extras.putLong("timestampUptimeMillis", SystemClock.uptimeMillis());
        return extras;
    }

    private void notifyWarmupIsDone(int uid, Runnable internalCallback) {
        ThreadUtils.assertOnUiThread();
        final Bundle args = new Bundle(); // Empty one - safe to reuse for all the callbacks.

        // Notifies all the sessions, as warmup() is tied to a UID, not a session.
        for (CustomTabsSessionToken session : mClientManager.uidToSessions(uid)) {
            // TODO(crbug.com/40932858): Remove extra callback after its usage dwindles down.
            safeExtraCallback(session, ON_WARMUP_COMPLETED, null);

            CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
            if (callback == null) continue;
            try {
                callback.onWarmupCompleted(args);
            } catch (Exception e) {
                // Catching all exceptions is really bad, but we need it here,
                // because Android exposes us to client bugs by throwing a variety
                // of exceptions. See crbug.com/517023.
                continue;
            }
        }

        if (internalCallback != null) {
            internalCallback.run();
        }

        logCallback("onWarmupCompleted()", bundleToJson(args).toString());
    }

    /**
     * Creates a Bundle with a value for navigation start and the specified page load metric.
     *
     * @param metricName Name of the page load metric.
     * @param navigationStartMicros Absolute navigation start time, in microseconds, in
     *         {@link SystemClock#uptimeMillis()} timebase.
     * @param offsetMs Offset in ms from navigationStart for the page load metric.
     *
     * @return A Bundle containing navigation start and the page load metric.
     */
    Bundle createBundleWithNavigationStartAndPageLoadMetric(
            String metricName, long navigationStartMicros, long offsetMs) {
        Bundle args = new Bundle();
        args.putLong(metricName, offsetMs);
        args.putLong(PageLoadMetrics.NAVIGATION_START, navigationStartMicros / 1000);
        return args;
    }

    /**
     * Notifies the application of a page load metric for a single metric.
     *
     * @param session Session identifier.
     * @param metricName Name of the page load metric.
     * @param navigationStartMicros Absolute navigation start time, in microseconds, in
     *         {@link SystemClock#uptimeMillis()} timebase.
     * @param offsetMs Offset in ms from navigationStart for the page load metric.
     *
     * @return Whether the metric has been dispatched to the client.
     */
    boolean notifySinglePageLoadMetric(
            CustomTabsSessionToken session,
            String metricName,
            long navigationStartMicros,
            long offsetMs) {
        return notifyPageLoadMetrics(
                session,
                createBundleWithNavigationStartAndPageLoadMetric(
                        metricName, navigationStartMicros, offsetMs));
    }

    /**
     * Notifies the application of a general page load metrics.
     *
     * TODD(lizeb): Move this to a proper method in {@link CustomTabsCallback} once one is
     * available.
     *
     * @param session Session identifier.
     * @param args Bundle containing metric information to update. Each item in the bundle
     *     should be a key specifying the metric name and the metric value as the value.
     */
    boolean notifyPageLoadMetrics(CustomTabsSessionToken session, Bundle args) {
        if (!mClientManager.shouldGetPageLoadMetrics(session)) return false;
        if (safeExtraCallback(session, PAGE_LOAD_METRICS_CALLBACK, args)) {
            logPageLoadMetricsCallback(args);
            return true;
        }
        return false;
    }

    /**
     * Notifies the application that the user has selected to open the page in their browser.
     *
     * @param session Session identifier.
     * @param webContents the WebContents of the tab being taken out of CCT.
     * @return true if success. To protect Chrome exceptions in the client application are swallowed
     *     and false is returned.
     */
    public boolean notifyOpenInBrowser(CustomTabsSessionToken session, WebContents webContents) {
        // Reset the client data header for the WebContents since it's not a CCT tab anymore.
        if (webContents != null) CustomTabsConnectionJni.get().setClientDataHeader(webContents, "");
        return safeExtraCallback(
                session,
                OPEN_IN_BROWSER_CALLBACK,
                getExtrasBundleForNavigationEventForSession(session));
    }

    /**
     * Wraps calling extraCallback in a try/catch so exceptions thrown by the host app don't crash
     * Chrome. See https://crbug.com/517023.
     */
    // The string passed is safe since it is a method name.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    protected boolean safeExtraCallback(
            CustomTabsSessionToken session, String callbackName, @Nullable Bundle args) {
        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return false;

        try (TraceEvent te =
                TraceEvent.scoped("CustomTabsConnection::safeExtraCallback", callbackName)) {
            callback.extraCallback(callbackName, args);
        } catch (Exception e) {
            return false;
        }
        return true;
    }

    /**
     * Calls {@link CustomTabsCallback#extraCallbackWithResult)}.
     * Wraps calling sendExtraCallbackWithResult in a try/catch so that exceptions thrown by the
     * host app don't crash Chrome.
     */
    @Nullable
    // The string passed is safe since it is a method name.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    public Bundle sendExtraCallbackWithResult(
            CustomTabsSessionToken session, String callbackName, @Nullable Bundle args) {
        CustomTabsCallback callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return null;

        try (TraceEvent te =
                TraceEvent.scoped(
                        "CustomTabsConnection::safeExtraCallbackWithResult", callbackName)) {
            return callback.extraCallbackWithResult(callbackName, args);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Keeps the application linked with a given session alive.
     *
     * The application is kept alive (that is, raised to at least the current process priority
     * level) until {@link #dontKeepAliveForSession} is called.
     *
     * @param session The Binder object identifying the session.
     * @param intent Intent describing the service to bind to.
     * @return true for success.
     */
    boolean keepAliveForSession(CustomTabsSessionToken session, Intent intent) {
        return mClientManager.keepAliveForSession(session, intent);
    }

    /**
     * Lets the lifetime of the process linked to a given sessionId be managed normally.
     *
     * Without a matching call to {@link #keepAliveForSession}, this is a no-op.
     *
     * @param session The Binder object identifying the session.
     */
    void dontKeepAliveForSession(CustomTabsSessionToken session) {
        mClientManager.dontKeepAliveForSession(session);
    }

    /**
     * Returns whether /proc/PID/ is accessible.
     *
     * On devices where /proc is mounted with the "hidepid=2" option, cannot get access to the
     * scheduler group, as this is under this directory, which is hidden unless PID == self (or
     * its numeric value).
     */
    @VisibleForTesting
    static boolean canGetSchedulerGroup(int pid) {
        String cgroupFilename = "/proc/" + pid;
        File f = new File(cgroupFilename);
        return f.exists() && f.isDirectory() && f.canExecute();
    }

    /**
     * @return the CPU cgroup of a given process, identified by its PID, or null.
     */
    @VisibleForTesting
    static String getSchedulerGroup(int pid) {
        // Android uses several cgroups for processes, depending on their priority. The list of
        // cgroups a process is part of can be queried by reading /proc/<pid>/cgroup, which is
        // world-readable.
        String cgroupFilename = "/proc/" + pid + "/cgroup";
        String controllerName = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? "cpuset" : "cpu";
        try (BufferedReader reader = new BufferedReader(new FileReader(cgroupFilename))) {
            String line = null;
            while ((line = reader.readLine()) != null) {
                // line format: 2:cpu:/bg_non_interactive
                String[] fields = line.trim().split(":");
                if (fields.length == 3 && fields[1].equals(controllerName)) return fields[2];
            }
        } catch (IOException e) {
            return null;
        }
        return null;
    }

    private static boolean isBackgroundProcess(int pid) {
        return BACKGROUND_GROUPS.contains(getSchedulerGroup(pid));
    }

    /**
     * @return true when inside a Binder transaction and the caller is in the
     * foreground or self. Don't use outside a Binder transaction.
     */
    private boolean isCallerForegroundOrSelf() {
        int uid = Binder.getCallingUid();
        if (uid == Process.myUid()) return true;

        // Starting with L MR1, AM.getRunningAppProcesses doesn't return all the processes so we use
        // a workaround in this case.
        int pid = Binder.getCallingPid();
        boolean workaroundAvailable = canGetSchedulerGroup(pid);
        // If we have no way to find out whether the calling process is in the foreground,
        // optimistically assume it is. Otherwise we would effectively disable CCT warmup
        // on these devices.
        if (!workaroundAvailable) return true;
        return isBackgroundProcess(pid);
    }

    void cleanupAllForTesting() {
        ThreadUtils.assertOnUiThread();
        mClientManager.cleanupAll();
        mHiddenTabHolder.destroyHiddenTab(null);
    }

    /**
     * Handle any clean up left after a session is destroyed.
     * @param session The session that has been destroyed.
     */
    @VisibleForTesting
    void cleanUpSession(final CustomTabsSessionToken session) {
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> mClientManager.cleanupSession(session));
    }

    /**
     * Discards substantial objects that are not currently in use.
     * @param level The type of signal as defined in {@link android.content.ComponentCallbacks2}.
     */
    public static void onTrimMemory(int level) {
        if (!hasInstance()) return;

        if (ChromeApplicationImpl.isSevereMemorySignal(level)) {
            getInstance().mClientManager.cleanupUnusedSessions();
        }
    }

    boolean maySpeculate(CustomTabsSessionToken session) {
        if (!DeviceClassManager.enablePrerendering()) {
            return false;
        }
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        if (UserPrefs.get(profile).getInteger(COOKIE_CONTROLS_MODE)
                == CookieControlsMode.BLOCK_THIRD_PARTY) {
            return false;
        }
        if (PreloadPagesSettingsBridge.getState(profile) == PreloadPagesState.NO_PRELOADING) {
            return false;
        }
        return true;
    }

    /** Cancels the speculation for a given session, or any session if null. */
    public void cancelSpeculation(@Nullable CustomTabsSessionToken session) {
        ThreadUtils.assertOnUiThread();
        mHiddenTabHolder.destroyHiddenTab(session);
    }

    /*
     * This function will do as much as it can to have a subsequent navigation
     * to the specified url sped up, including speculatively loading a url, preconnecting,
     * and starting a spare renderer.
     */
    private void startSpeculation(
            CustomTabsSessionToken session,
            String url,
            boolean useHiddenTab,
            Bundle extras,
            int uid,
            boolean useSeparateStoragePartitionForExperiment) {
        WarmupManager warmupManager = WarmupManager.getInstance();
        Profile profile = ProfileManager.getLastUsedRegularProfile();

        // At most one on-going speculation, clears the previous one.
        cancelSpeculation(null);

        if (useHiddenTab) {
            launchUrlInHiddenTab(
                    session, profile, url, extras, useSeparateStoragePartitionForExperiment);
        } else {
            createSpareWebContents(profile);
        }
        warmupManager.maybePreconnectUrlAndSubResources(profile, url);
    }

    /** Creates a hidden tab and initiates a navigation. */
    private void launchUrlInHiddenTab(
            CustomTabsSessionToken session,
            Profile profile,
            String url,
            @Nullable Bundle extras,
            boolean useSeparateStoragePartitionForExperiment) {
        ThreadUtils.assertOnUiThread();

        WebContents webContents = null;

        if (useSeparateStoragePartitionForExperiment) {
            webContents =
                    WebContentsFactory.createWebContentsWithSeparateStoragePartitionForExperiment(
                            profile);
        }

        mHiddenTabHolder.launchUrlInHiddenTab(
                (Tab tab) -> setClientDataHeaderForNewTab(session, tab.getWebContents()),
                session,
                profile,
                mClientManager,
                url,
                extras,
                webContents);
    }

    @VisibleForTesting
    void resetThrottling(int uid) {
        mClientManager.resetThrottling(uid);
    }

    @VisibleForTesting
    void ban(int uid) {
        mClientManager.ban(uid);
    }

    /**
     * @return The referrer that is associated with the client owning the given session.
     */
    public Referrer getDefaultReferrerForSession(CustomTabsSessionToken session) {
        return mClientManager.getDefaultReferrerForSession(session);
    }

    /**
     * @return The package name of a client for which the publisher URL from a trusted CDN can be
     *         shown, or null to disallow showing the publisher URL.
     */
    public @Nullable String getTrustedCdnPublisherUrlPackage() {
        return mTrustedPublisherUrlPackage;
    }

    /**
     * @return Whether the publisher of the URL from a trusted CDN can be shown.
     */
    public boolean isTrustedCdnPublisherUrlPackage(@Nullable String urlPackage) {
        return urlPackage != null && urlPackage.equals(getTrustedCdnPublisherUrlPackage());
    }

    void setTrustedPublisherUrlPackageForTest(@Nullable String packageName) {
        mTrustedPublisherUrlPackage = packageName;
    }

    public void setEngagementSignalsAvailableSupplier(
            CustomTabsSessionToken session, Supplier<Boolean> supplier) {
        mClientManager.setEngagementSignalsAvailableSupplierForSession(session, supplier);
    }

    public EngagementSignalsHandler getEngagementSignalsHandler(CustomTabsSessionToken session) {
        return mClientManager.getEngagementSignalsHandlerForSession(session);
    }

    @CalledByNative
    public static void notifyClientOfDetachedRequestCompletion(
            CustomTabsSessionToken session, String url, int status) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)) {
            return;
        }
        Bundle args = new Bundle();
        args.putParcelable("url", Uri.parse(url));
        args.putInt("net_error", status);
        CustomTabsConnection connection = getInstance();
        connection.safeExtraCallback(session, ON_DETACHED_REQUEST_COMPLETED, args);
        if (connection.mLogRequests) {
            connection.logCallback(ON_DETACHED_REQUEST_COMPLETED, bundleToJson(args).toString());
        }
    }

    @VisibleForTesting
    @Nullable
    HiddenTabHolder.SpeculationParams getSpeculationParamsForTesting() {
        return mHiddenTabHolder.getSpeculationParamsForTesting();
    }

    public static void createSpareWebContents(Profile profile) {
        if (sSkipTabPrewarmingForTesting) return;
        if (SysUtils.isLowEndDevice()) return;
        if (WarmupManager.getInstance().isCCTPrewarmTabFeatureEnabled(true)) {
            WarmupManager.getInstance().createRegularSpareTab(profile);
        } else {
            WarmupManager.getInstance().createSpareWebContents(profile);
        }
    }

    public boolean receiveFile(
            CustomTabsSessionToken sessionToken, Uri uri, int purpose, Bundle extras) {
        return ChromeApplicationImpl.getComponent()
                .resolveCustomTabsFileProcessor()
                .processFile(sessionToken, uri, purpose, extras);
    }

    public void setCustomTabIsInForeground(
            @Nullable CustomTabsSessionToken session, boolean isInForeground) {
        mClientManager.setCustomTabIsInForeground(session, isInForeground);
    }

    public boolean isEngagementSignalsApiAvailable(
            CustomTabsSessionToken sessionToken, Bundle extras) {
        return isEngagementSignalsApiAvailableInternal(sessionToken);
    }

    public boolean setEngagementSignalsCallback(
            CustomTabsSessionToken sessionToken,
            EngagementSignalsCallback callback,
            Bundle extras) {
        if (!isEngagementSignalsApiAvailableInternal(sessionToken)) return false;

        var engagementSignalsHandler =
                mClientManager.getEngagementSignalsHandlerForSession(sessionToken);
        if (engagementSignalsHandler == null) return false;

        mClientManager.setEngagementSignalsCallbackForSession(sessionToken, callback);
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> engagementSignalsHandler.setEngagementSignalsCallback(callback));
        return true;
    }

    private boolean isEngagementSignalsApiAvailableInternal(CustomTabsSessionToken session) {
        var supplier = mClientManager.getEngagementSignalsAvailableSupplierForSession(session);
        return supplier != null
                ? supplier.get()
                : PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    public boolean hasEngagementSignalsCallback(CustomTabsSessionToken session) {
        return mClientManager.getEngagementSignalsCallbackForSession(session) != null;
    }

    /** Whether a CustomTabs instance should include interactive Omnibox. */
    public boolean shouldEnableOmniboxForIntent(BrowserServicesIntentDataProvider intentData) {
        return false;
    }

    /**
     * Returns an alternate handler for taps on the Custom Tabs Omnibox, or null if the default
     * handler should be used.
     */
    @Nullable
    public Consumer<Tab> getAlternateOmniboxTapHandler(
            BrowserServicesIntentDataProvider intentData) {
        return null;
    }

    /** Specifies what content should be presented by the CustomTabs instance in location bar. */
    public int getTitleVisibilityState(BrowserServicesIntentDataProvider intentData) {
        if (shouldEnableOmniboxForIntent(intentData)) {
            return CustomTabsIntent.NO_TITLE;
        }
        return intentData.getTitleVisibilityState();
    }

    /**
     * Whether Google Bottom Bar is enabled by the launching Intent. False by default.
     *
     * @param intentData {@link BrowserServicesIntentDataProvider} built from the Intent that
     *     launched this CCT.
     */
    public boolean shouldEnableGoogleBottomBarForIntent(
            BrowserServicesIntentDataProvider intentData) {
        return false;
    }

    /**
     * Checks whether Google Bottom Bar buttons are present in the Intent data. False by default.
     *
     * @param intentData {@link BrowserServicesIntentDataProvider} built from the Intent that
     *     launched this CCT.
     */
    public boolean hasExtraGoogleBottomBarButtons(BrowserServicesIntentDataProvider intentData) {
        return false;
    }

    /**
     * Returns Google Bottom Bar buttons that are added to the Intent.
     *
     * @param intentData {@link BrowserServicesIntentDataProvider} built from the Intent that
     *     launched this CCT.
     * @return An ArrayList of Bundles, each representing a Google Bottom Bar item.
     */
    public List<Bundle> getGoogleBottomBarButtons(BrowserServicesIntentDataProvider intentData) {
        return new ArrayList<>();
    }

    public GoogleBottomBarIntentParams getGoogleBottomBarIntentParams(
            BrowserServicesIntentDataProvider intentData) {
        return GoogleBottomBarIntentParams.getDefaultInstance();
    }

    /**
     * Called when text fragment lookups on the current page has completed.
     *
     * @param session session object.
     * @param stateKey unique key for the embedder to keep track of the request.
     * @param foundTextFragments text fragments from the initial request that were found on the
     *     page.
     */
    @CalledByNative
    private static void notifyClientOfTextFragmentLookupCompletion(
            CustomTabsSessionToken session, String stateKey, String[] foundTextFragments) {
        getInstance()
                .notifyClientOfTextFragmentLookupCompletionReportApp(
                        session, stateKey, new ArrayList(Arrays.asList(foundTextFragments)));
    }

    protected void notifyClientOfTextFragmentLookupCompletionReportApp(
            CustomTabsSessionToken session,
            String stateKey,
            ArrayList<String> foundTextFragments) {}

    /**
     * @return The CalledWarmup state for the session.
     */
    public @CalledWarmup int getWarmupState(CustomTabsSessionToken session) {
        return mClientManager.getWarmupState(session);
    }

    public static void setInstanceForTesting(CustomTabsConnection connection) {
        var oldValue = sInstance;
        sInstance = connection;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @NativeMethods
    interface Natives {
        void createAndStartDetachedResourceRequest(
                @JniType("Profile*") Profile profile,
                CustomTabsSessionToken session,
                String packageName,
                String url,
                String origin,
                int referrerPolicy,
                @DetachedResourceRequestMotivation int motivation);

        void setClientDataHeader(WebContents webContents, String header);

        void textFragmentLookup(
                CustomTabsSessionToken session,
                WebContents webContents,
                String stateKey,
                String[] textFragment);

        void textFragmentFindScrollAndHighlight(
                CustomTabsSessionToken session, WebContents webContents, String textFragment);
    }
}
