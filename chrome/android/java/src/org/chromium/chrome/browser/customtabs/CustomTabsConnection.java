// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.app.PendingIntent;
import android.content.ComponentCallbacks2;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.widget.RemoteViews;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.ExperimentalPrefetch;
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
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserCallbackWrapper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TitleVisibility;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.customtabs.ClientManager.CalledWarmup;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.customtabs.features.branding.MismatchNotificationData;
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
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;

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
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Implementation of the ICustomTabsService interface.
 *
 * <p>Note: This class is meant to be package private, and is public to be accessible from {@link
 * ChromeApplicationImpl}.
 */
@JNINamespace("customtabs")
@NullMarked
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

    @VisibleForTesting
    static final String PARALLEL_REQUEST_URL_LIST_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_URL_LIST";

    static final String RESOURCE_PREFETCH_URL_LIST_KEY =
            "androidx.browser.RESOURCE_PREFETCH_URL_LIST";

    private static final String ON_RESIZED_CALLBACK = "onResized";
    private static final String ON_RESIZED_SIZE_EXTRA = "size";

    static final String IS_AUTH_TAB_SUPPORTED = "isAuthTabSupported";
    static final String AUTH_TAB_SUPPORTED_KEY = "authTabSupported";

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

    // NOTE: This must be kept in sync with the definitions in CustomTabsCallback.java in AndroidX
    // browser lib and the enums in /tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    // LINT.IfChange(CustomTabsNavigationEvent)
    @IntDef({
        CustomTabsNavigationEvent.NAVIGATION_STARTED,
        CustomTabsNavigationEvent.NAVIGATION_FINISHED,
        CustomTabsNavigationEvent.NAVIGATION_FAILED,
        CustomTabsNavigationEvent.NAVIGATION_ABORTED,
        CustomTabsNavigationEvent.TAB_SHOWN,
        CustomTabsNavigationEvent.TAB_HIDDEN
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CustomTabsNavigationEvent {
        int NAVIGATION_STARTED = CustomTabsCallback.NAVIGATION_STARTED;
        int NAVIGATION_FINISHED = CustomTabsCallback.NAVIGATION_FINISHED;
        int NAVIGATION_FAILED = CustomTabsCallback.NAVIGATION_FAILED;
        int NAVIGATION_ABORTED = CustomTabsCallback.NAVIGATION_ABORTED;
        int TAB_SHOWN = CustomTabsCallback.TAB_SHOWN;
        int TAB_HIDDEN = CustomTabsCallback.TAB_HIDDEN;

        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/custom_tabs/enums.xml:CustomTabsNavigationEvent)

    private static @Nullable CustomTabsConnection sInstance;
    private @Nullable String mTrustedPublisherUrlPackage;

    private final HiddenTabHolder mHiddenTabHolder = new HiddenTabHolder();
    protected final SessionDataHolder mSessionDataHolder;

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    final ClientManager mClientManager;

    protected final boolean mLogRequests;
    private final AtomicBoolean mWarmupHasBeenCalled = new AtomicBoolean();
    private final AtomicBoolean mWarmupHasBeenFinished = new AtomicBoolean();

    private @Nullable Callback<SessionHolder<?>> mDisconnectCallback;

    private volatile @Nullable ChainedTasks mWarmupTasks;

    // Caches the previous height reported via |onResized|. Used for extraCallback
    // |ON_RESIZED_CALLLBACK| which cares about height only.
    private int mPrevHeight;

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
        mSessionDataHolder = SessionDataHolder.getInstance();
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
    protected static JSONObject bundleToJson(@Nullable Bundle bundle) {
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

    /**
     * Logging for page load metrics callback, if service has enabled logging.
     *
     * <p>No rate-limiting, can be spammy if the app is misbehaved.
     *
     * @param args arguments of the callback.
     */
    void logPageLoadMetricsCallback(Bundle args) {
        if (!mLogRequests) return; // Don't build args if not necessary.
        logCallback(
                "extraCallback(" + PAGE_LOAD_METRICS_CALLBACK + ")", bundleToJson(args).toString());
    }

    /** Sets a callback to be triggered when a service connection is terminated. */
    public void setDisconnectCallback(@Nullable Callback<SessionHolder<?>> callback) {
        mDisconnectCallback = callback;
    }

    public boolean newSession(CustomTabsSessionToken session) {
        boolean success = false;
        if (session != null) {
            SessionHolder<CustomTabsSessionToken> holder = new SessionHolder<>(session);
            success = newSessionInternal(holder);
        }
        logCall("newSession()", success);
        return success;
    }

    private boolean newSessionInternal(SessionHolder<?> session) {
        ClientManager.DisconnectCallback onDisconnect =
                new ClientManager.DisconnectCallback() {
                    @Override
                    public void run(SessionHolder<?> session) {
                        cancelSpeculation(session);
                        if (mDisconnectCallback != null) {
                            mDisconnectCallback.onResult(session);
                        }

                        // TODO(pshmakov): invert this dependency by moving event dispatching to a
                        // separate class.
                        CustomTabsClientFileProcessor.getInstance().onSessionDisconnected(session);
                    }
                };

        PostMessageServiceConnection serviceConnection = null;
        PostMessageHandler postMessageHandler = null;
        EngagementSignalsHandler engagementSignalsHandler = null;
        if (session.isCustomTab()) {
            var customTabSession = session.getSessionAsCustomTab();
            // TODO(peconn): Make this not an anonymous class once PostMessageServiceConnection is
            // made
            // non-abstract in AndroidX.
            serviceConnection = new PostMessageServiceConnection(customTabSession) {};
            postMessageHandler = new PostMessageHandler(serviceConnection);
            engagementSignalsHandler = new EngagementSignalsHandler(customTabSession);
        }
        return mClientManager.newSession(
                session,
                Binder.getCallingUid(),
                onDisconnect,
                postMessageHandler,
                serviceConnection,
                engagementSignalsHandler);
    }

    /**
     * Overrides the given session's packageName if it is generated by Chrome. To be used for
     * testing only. To be called before the session given is associated with a tab.
     *
     * @param session The session for which the package name should be overridden.
     * @param packageName The new package name to set.
     */
    public void overridePackageNameForSessionForTesting(
            SessionHolder<?> session, String packageName) {
        String originalPackage = getClientPackageNameForSession(session);
        String selfPackage = ContextUtils.getApplicationContext().getPackageName();
        if (TextUtils.isEmpty(originalPackage) || !selfPackage.equals(originalPackage)) return;
        mClientManager.overridePackageNameForSessionForTesting(session, packageName); // IN-TEST
    }

    public boolean warmup() {
        return warmup(null);
    }

    public boolean warmup(@Nullable Runnable completionCallback) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.warmup")) {
            boolean success = warmupInternal(completionCallback);
            logCall("warmup()", success);
            return success;
        }
    }

    /**
     * @return Whether native initialization has finished.
     */
    public boolean hasWarmUpBeenFinished() {
        if (ChromeFeatureList.sCctFixWarmup.isEnabled()) {
            return ChromeBrowserInitializer.getInstance().isFullBrowserInitialized();
        } else {
            return mWarmupHasBeenFinished.get();
        }
    }

    /**
     * Starts as much as possible in anticipation of a future navigation.
     *
     * @param completionCallback callback to be called after all processes are finished.
     * @return true for success.
     */
    private boolean warmupInternal(@Nullable Runnable completionCallback) {
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
        final boolean fixWarmupEnabled = ChromeFeatureList.sCctFixWarmup.isEnabled();
        boolean shouldStartBrowser =
                fixWarmupEnabled
                        && !ChromeBrowserInitializer.getInstance().isFullBrowserInitialized();
        boolean legacyShouldStartBrowser = !fixWarmupEnabled && !initialized;
        if (shouldStartBrowser || legacyShouldStartBrowser) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        try (TraceEvent e =
                                TraceEvent.scoped("CustomTabsConnection.initializeBrowser()")) {
                            ChromeBrowserInitializer.getInstance()
                                    .handleSynchronousStartupWithGpuWarmUp();
                            ProcessInitializationHandler.getInstance().initNetworkChangeNotifier();
                            if (legacyShouldStartBrowser) mWarmupHasBeenFinished.set(true);
                        }
                    });
        }

        // (2)
        if (!mHiddenTabHolder.hasHiddenTab()) {
            tasks.add(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mHiddenTabHolder.hasHiddenTab()) return;

                        // TODO(https://crbug.com/423415329): I'm pretty sure this is fixed, just
                        // rolling this out with the flagged change in case it isn't fixed.
                        if (!fixWarmupEnabled) {
                            // Temporary fix for https://crbug.com/797832.
                            // TODO(lizeb): Properly fix instead of papering over the bug, this code
                            // should not be scheduled unless startup is done. See
                            // https://crbug.com/797832.
                            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                                return;
                            }
                        }
                        try (TraceEvent e = TraceEvent.scoped("CreateSpareTab")) {
                            createSpareTab(ProfileManager.getLastUsedRegularProfile());
                        }
                    });
        }

        // (3)
        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try (TraceEvent e = TraceEvent.scoped("InitializeViewHierarchy")) {
                        int toolbarLayoutId =
                                ChromeFeatureList.sCctToolbarRefactor.isEnabled()
                                        ? R.layout.new_custom_tab_toolbar
                                        : R.layout.custom_tabs_toolbar;
                        WarmupManager.getInstance()
                                .initializeViewHierarchy(
                                        ContextUtils.getApplicationContext(),
                                        R.layout.custom_tabs_control_container,
                                        toolbarLayoutId);
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

        tasks.add(TaskTraits.UI_DEFAULT, () -> notifyWarmupIsDone(uid, completionCallback));
        tasks.start(false);
        mWarmupTasks = tasks;
        return true;
    }

    /** @return the URL or null if it's invalid. */
    @Contract("null -> false")
    private static boolean isValid(@Nullable Uri uri) {
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
     * High confidence mayLaunchUrl() call, that is: - Tries to speculate if possible. - An empty
     * URL cancels the current prerender if any. - Start a spare renderer if necessary.
     */
    private void highConfidenceMayLaunchUrl(
            SessionHolder<?> session,
            @Nullable String url,
            @Nullable Bundle extras,
            @Nullable List<Bundle> otherLikelyBundles) {
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
    boolean lowConfidenceMayLaunchUrl(@Nullable List<Bundle> likelyBundles) {
        ThreadUtils.assertOnUiThread();
        if (!preconnectUrls(likelyBundles)) return false;
        createSpareTab(ProfileManager.getLastUsedRegularProfile());
        return true;
    }

    public @Nullable Tab getHiddenTabForTesting() {
        return mHiddenTabHolder != null ? mHiddenTabHolder.getHiddenTabForTesting() : null;
    }

    private boolean preconnectUrls(@Nullable List<Bundle> likelyBundles) {
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
            @Nullable Uri url,
            @Nullable Bundle extras,
            @Nullable List<Bundle> otherLikelyBundles) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.mayLaunchUrl")) {
            boolean success =
                    mayLaunchUrlInternal(
                            new SessionHolder<>(session), url, extras, otherLikelyBundles);
            logCall("mayLaunchUrl(" + url + ")", success);
            return success;
        }
    }

    private boolean mayLaunchUrlInternal(
            final SessionHolder<?> session,
            final @Nullable Uri url,
            final @Nullable Bundle extras,
            final @Nullable List<Bundle> otherLikelyBundles) {
        // mayLaunchUrl should not be executed for Incognito CCT since all setup is created with
        // regular profile. If we need to enable mayLaunchUrl for off-the-record profiles, we need
        // to update the profile used. Please see crbug.com/1106757.
        if (IntentHandler.hasAnyIncognitoExtra(extras)) return false;

        final boolean lowConfidence =
                (url == null || TextUtils.isEmpty(url.toString())) && otherLikelyBundles != null;
        final String urlString = isValid(url) ? url.toString() : null;
        if (url != null && urlString == null && !lowConfidence) return false;

        final int uid = Binder.getCallingUid();

        if (!warmupInternal(null)) return false;

        if (!mClientManager.updateStatsAndReturnWhetherAllowed(
                session, uid, urlString, otherLikelyBundles != null)) {
            return false;
        }

        // Run after the first chained warmup task completes and native is initialized.
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

    @ExperimentalPrefetch
    public void prefetch(CustomTabsSessionToken session, List<Uri> urls, PrefetchOptions options) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.prefetch")) {
            if (!ChromeFeatureList.sCctNavigationalPrefetch.isEnabled()) {
                Log.w(TAG, "CCTNavigationalPrefetch is not enabled.");
                return;
            }
            RecordHistogram.recordBooleanHistogram("CustomTabs.Prefetch.PrefetchCalled", true);
            prefetchInternal(new SessionHolder<>(session), urls, options);
        }
    }

    @ExperimentalPrefetch
    private void prefetchInternal(
            SessionHolder<?> session, List<Uri> urls, PrefetchOptions options) {
        boolean usePrefetchProxy = options.requiresAnonymousIpWhenCrossOrigin;
        Origin sourceOrigin =
                options.sourceOrigin != null
                        ? Origin.create(options.sourceOrigin.toString())
                        : null;

        // We should call
        // (1) warmupInternal to initialize browser and prepare spare WebContents for (2), (3)
        // (2) validateSourceOriginOfPrefetch to register source origin of prefetch to
        //     OriginVerifier
        // (3) startPrefetchFromCct
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
                                            .startPrefetchFromCct(
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
        warmupInternal(validateOrigin);
    }

    @VisibleForTesting
    @ExperimentalPrefetch
    @Contract("_, null -> false")
    boolean isValidForPrefetchSourceOrigin(SessionHolder<?> session, @Nullable Origin origin) {
        return origin != null && mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    private void enableExperimentIdsIfNecessary(@Nullable Bundle extras) {
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
            final SessionHolder<?> session,
            final int uid,
            final @Nullable String urlString,
            final @Nullable Bundle extras,
            final @Nullable List<Bundle> otherLikelyBundles,
            boolean retryIfNotLoaded) {
        ThreadUtils.assertOnUiThread();
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.mayLaunchUrlOnUiThread")) {
            // TODO(https://crbug.com/423415329): I'm pretty sure this is fixed, just
            // rolling this out with the flagged change in case it isn't fixed.
            if (!ChromeFeatureList.sCctFixWarmup.isEnabled()) {
                // doMayLaunchUrlInternal() is always called once the native level initialization is
                // done, at least the initial profile load. However, at that stage the startup
                // callback
                // may not have run, which causes ProfileManager.getLastUsedRegularProfile() to
                // throw an
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
            }

            enableExperimentIdsIfNecessary(extras);

            if (lowConfidence) {
                lowConfidenceMayLaunchUrl(otherLikelyBundles);
            } else {
                highConfidenceMayLaunchUrl(session, urlString, extras, otherLikelyBundles);
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
    public @Nullable Bundle extraCommand(String commandName, @Nullable Bundle args) {
        if (commandName.equals(IS_AUTH_TAB_SUPPORTED)) {
            var bundle = new Bundle();
            boolean supported = ChromeFeatureList.sCctAuthTab.isEnabled();
            bundle.putBoolean(AUTH_TAB_SUPPORTED_KEY, supported);
            return bundle;
        }
        return null;
    }

    public boolean updateVisuals(final CustomTabsSessionToken session, @Nullable Bundle bundle) {
        if (mLogRequests) Log.w(TAG, "updateVisuals: %s", bundleToJson(bundle));
        SessionHandler handler = mSessionDataHolder.getActiveHandler(new SessionHolder<>(session));
        if (handler == null) return false;
        assert bundle != null;

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

    private static @Nullable PendingIntent getSecondarySwipeToolbarSwipeUpGesture(Bundle bundle) {
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
                        new SessionHolder<>(session),
                        postMessageSourceOrigin,
                        postMessageTargetOrigin);
        logCall(
                "requestPostMessageChannel() with origin "
                        + (postMessageSourceOrigin != null
                                ? postMessageSourceOrigin.toString()
                                : ""),
                success);
        return success;
    }

    private boolean requestPostMessageChannelInternal(
            final SessionHolder<?> session,
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
     *
     * @param session The session to use for getting client information.
     * @param clientUid The UID for the client controlling the session.
     * @param origin The origin that is suggested by the client. The validated origin may be this or
     *     a derivative of this.
     * @return The validated origin {@link Uri} for the given session's client.
     */
    protected @Nullable Uri verifyOriginForSession(
            SessionHolder<?> session, int clientUid, Origin origin) {
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
        SessionHolder<?> session = SessionHolder.getSessionHolderFromIntent(intent);
        if (session == null) return false;

        Origin origin = Origin.create(intent.getData());
        if (origin == null) return false;

        return mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    public int postMessage(
            CustomTabsSessionToken session, String message, @Nullable Bundle extras) {
        var sessionHolder = new SessionHolder<>(session);
        int result;
        if (!mWarmupHasBeenCalled.get()) result = CustomTabsService.RESULT_FAILURE_DISALLOWED;
        if (!isCallerForegroundOrSelf() && !mSessionDataHolder.isActiveSession(sessionHolder)) {
            result = CustomTabsService.RESULT_FAILURE_DISALLOWED;
        }
        // If called before a validatePostMessageOrigin, the post message origin will be invalid and
        // will return a failure result here.
        result = mClientManager.postMessage(sessionHolder, message);
        logCall("postMessage", result);
        return result;
    }

    public boolean validateRelationship(
            CustomTabsSessionToken sessionToken,
            int relation,
            Origin origin,
            @Nullable Bundle extras) {
        var session = new SessionHolder<>(sessionToken);
        // Essential parts of the verification will depend on native code and will be run sync on UI
        // thread. Make sure the client has called warmup() beforehand.
        if (!mWarmupHasBeenCalled.get()) {
            Log.d(TAG, "Verification failed due to warmup not having been previously called.");
            assumeNonNull(mClientManager.getCallbackForSession(session))
                    .onRelationshipValidationResult(
                            relation, Uri.parse(origin.toString()), false, null);
            return false;
        }
        return mClientManager.validateRelationship(session, relation, origin, extras);
    }

    /** See {@link ClientManager#resetPostMessageHandlerForSession(SessionHolder, WebContents)}. */
    public void resetPostMessageHandlerForSession(
            SessionHolder<?> session, WebContents webContents) {
        mClientManager.resetPostMessageHandlerForSession(session, webContents);
    }

    /**
     * Registers a launch of a |url| for a given |session|.
     *
     * <p>This is used for accounting.
     */
    void registerLaunch(@Nullable SessionHolder<?> session, String url) {
        mClientManager.registerLaunch(session, url);
    }

    /**
     * Returns the preloaded {@link Tab} if it matches the given |url| and |referrer|. Null if no
     * such {@link Tab}. If a {@link Tab} is preloaded but it does not match, it is discarded.
     *
     * @param session The Binder object identifying a session.
     * @param url The URL the tab is for.
     * @param referrer The referrer to use for |url|.
     * @param intentDataProvider The {@link BrowserServicesIntentDataProvider} created from the
     *     Custom Tabs Intent.
     * @return The hidden tab, or null.
     */
    public HiddenTabHolder.@Nullable HiddenTab takeHiddenTab(
            @Nullable SessionHolder<?> session,
            String url,
            BrowserServicesIntentDataProvider intentDataProvider) {
        return mHiddenTabHolder.takeHiddenTab(
                session,
                mClientManager.getIgnoreFragmentsForSession(session),
                url,
                intentDataProvider);
    }

    /**
     * Called when an intent is handled by either an existing or a new CustomTabActivity.
     *
     * @param session Session extracted from the intent.
     * @param intent incoming intent.
     */
    public void onHandledIntent(@Nullable SessionHolder<?> session, Intent intent) {
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

        try (TraceEvent event = TraceEvent.scoped("CustomTabsConnection.PreconnectResources")) {
            maybePreconnectToRedirectEndpoint(session, url, intent);
            ChromeBrowserInitializer.getInstance()
                    .runNowOrAfterFullBrowserStarted(() -> handleParallelRequest(session, intent));
            maybePrefetchResources(session, intent);
        }
    }

    /**
     * Called each time a CCT tab is created to check if a client data header was set and if so
     * forward it along to the native side.
     *
     * @param session Session identifier.
     * @param webContents the WebContents of the new tab.
     */
    public void setClientDataHeaderForNewTab(
            SessionHolder<?> session, @Nullable WebContents webContents) {}

    protected void setClientDataHeader(WebContents webContents, String header) {
        if (TextUtils.isEmpty(header)) return;

        CustomTabsConnectionJni.get().setClientDataHeader(webContents, header);
    }

    private void maybePreconnectToRedirectEndpoint(
            @Nullable SessionHolder<?> session, String url, Intent intent) {
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
    int handleParallelRequest(@Nullable SessionHolder<?> session, Intent intent) {
        int status = maybeStartParallelRequest(session, intent);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.ParallelRequestStatusOnStart",
                status,
                ParallelRequestStatus.NUM_ENTRIES);

        if (mLogRequests) {
            Log.w(TAG, "handleParallelRequest() = " + PARALLEL_REQUEST_MESSAGES[status]);
        }

        // Success is already reported per URL, report any failures here.
        if ((status != ParallelRequestStatus.SUCCESS)) {
            reportParallelRequestStatus(
                    session, status, intent.getParcelableExtra(PARALLEL_REQUEST_URL_KEY));
        }

        return status;
    }

    private void reportParallelRequestStatus(
            @Nullable SessionHolder<?> session,
            @ParallelRequestStatus int status,
            @Nullable Uri url) {
        if ((status == ParallelRequestStatus.NO_REQUEST)
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)) {
            return;
        }
        Bundle args = new Bundle();
        args.putParcelable("url", url);
        args.putInt("status", status);
        safeExtraCallback(session, ON_DETACHED_REQUEST_REQUESTED, args);
        if (mLogRequests) {
            logCallback(ON_DETACHED_REQUEST_REQUESTED, bundleToJson(args).toString());
        }
    }

    /**
     * Maybe starts a parallel request.
     *
     * @param session Calling context session.
     * @param intent Incoming intent with the extras.
     * @return Whether the request was started, with reason in case of failure.
     */
    private @ParallelRequestStatus int maybeStartParallelRequest(
            @Nullable SessionHolder<?> session, Intent intent) {
        ThreadUtils.assertOnUiThread();

        if (!intent.hasExtra(PARALLEL_REQUEST_URL_KEY)
                && !intent.hasExtra(PARALLEL_REQUEST_URL_LIST_KEY)) {
            return ParallelRequestStatus.NO_REQUEST;
        }
        if (!ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
            return ParallelRequestStatus.FAILURE_NOT_INITIALIZED;
        }
        if (intent.hasExtra(PARALLEL_REQUEST_URL_LIST_KEY)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MULTIPLE_PARALLEL_REQUESTS)) {
            return ParallelRequestStatus.NO_REQUEST;
        }
        String packageName = mClientManager.getClientPackageNameForSession(session);
        if (session == null
                || packageName == null
                || !mClientManager.getAllowParallelRequestForSession(session)) {
            return ParallelRequestStatus.FAILURE_NOT_AUTHORIZED;
        }

        Uri referrer = IntentUtils.safeGetParcelableExtra(intent, PARALLEL_REQUEST_REFERRER_KEY);
        int policy =
                intent.getIntExtra(PARALLEL_REQUEST_REFERRER_POLICY_KEY, ReferrerPolicy.DEFAULT);
        if (referrer == null) return ParallelRequestStatus.FAILURE_INVALID_REFERRER;
        if (policy < ReferrerPolicy.MIN_VALUE || policy > ReferrerPolicy.MAX_VALUE) {
            policy = ReferrerPolicy.DEFAULT;
        }

        if (!canDoParallelRequest(session, referrer)) {
            return ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
        }

        String referrerString = referrer.toString();
        Uri uri = intent.getParcelableExtra(PARALLEL_REQUEST_URL_KEY);
        if (uri != null) {
            return doParallelResourceRequest(session, uri, referrerString, packageName, policy);
        }

        List<Uri> urls =
                IntentUtils.getParcelableArrayListExtra(intent, PARALLEL_REQUEST_URL_LIST_KEY);
        if (urls == null) return ParallelRequestStatus.FAILURE_INVALID_URL;
        for (Uri url : urls) {
            @ParallelRequestStatus
            int result =
                    doParallelResourceRequest(session, url, referrerString, packageName, policy);
            if (result != ParallelRequestStatus.SUCCESS) return result;
        }
        return ParallelRequestStatus.SUCCESS;
    }

    private @ParallelRequestStatus int doParallelResourceRequest(
            SessionHolder<?> session, Uri url, String referrer, String packageName, int policy) {
        if (url.toString().equals("") || !isValid(url)) {
            return ParallelRequestStatus.FAILURE_INVALID_URL;
        }
        String urlString = url.toString();

        CustomTabsConnectionJni.get()
                .createAndStartDetachedResourceRequest(
                        ProfileManager.getLastUsedRegularProfile(),
                        session,
                        packageName,
                        urlString,
                        referrer,
                        policy,
                        DetachedResourceRequestMotivation.PARALLEL_REQUEST);
        if (mLogRequests) {
            Log.w(TAG, "startParallelRequest(%s, %s, %d)", urlString, referrer, policy);
        }
        reportParallelRequestStatus(session, ParallelRequestStatus.SUCCESS, url);
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
    int maybePrefetchResources(@Nullable SessionHolder<?> session, Intent intent) {
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
     * @return Whether {@code session} can create a parallel request for a given {@code referrer}.
     */
    @VisibleForTesting
    boolean canDoParallelRequest(@Nullable SessionHolder<?> session, Uri referrer) {
        ThreadUtils.assertOnUiThread();
        Origin origin = Origin.create(referrer);
        if (origin == null) return false;
        return mClientManager.isFirstPartyOriginForSession(session, origin);
    }

    /**
     * @see ClientManager#shouldHideDomainForSession(SessionHolder)
     */
    public boolean shouldHideDomainForSession(SessionHolder<?> session) {
        return mClientManager.shouldHideDomainForSession(session);
    }

    /**
     * @see ClientManager#shouldSpeculateLoadOnCellularForSession(SessionHolder)
     */
    public boolean shouldSpeculateLoadOnCellularForSession(SessionHolder<?> session) {
        return mClientManager.shouldSpeculateLoadOnCellularForSession(session);
    }

    /**
     * @see ClientManager#getCanUseHiddenTab(SessionHolder)
     */
    public boolean canUseHiddenTabForSession(SessionHolder<?> session) {
        return mClientManager.getCanUseHiddenTab(session);
    }

    /**
     * @see ClientManager#shouldSendNavigationInfoForSession(SessionHolder)
     */
    public boolean shouldSendNavigationInfoForSession(@Nullable SessionHolder<?> session) {
        return mClientManager.shouldSendNavigationInfoForSession(session);
    }

    /**
     * @see ClientManager#shouldSendBottomBarScrollStateForSession(SessionHolder)
     */
    public boolean shouldSendBottomBarScrollStateForSession(SessionHolder<?> session) {
        return mClientManager.shouldSendBottomBarScrollStateForSession(session);
    }

    /** See {@link ClientManager#getClientPackageNameForSession(SessionHolder)} */
    public @Nullable String getClientPackageNameForSession(@Nullable SessionHolder<?> session) {
        return mClientManager.getClientPackageNameForSession(session);
    }

    /**
     * @return Whether the given package name is that of a first-party application.
     */
    public boolean isFirstParty(@Nullable String packageName) {
        if (packageName == null) return false;
        return ExternalAuthUtils.getInstance().isGoogleSigned(packageName);
    }

    void setIgnoreUrlFragmentsForSession(SessionHolder<?> session, boolean value) {
        mClientManager.setIgnoreFragmentsForSession(session, value);
    }

    @VisibleForTesting
    boolean getIgnoreUrlFragmentsForSession(SessionHolder<?> session) {
        return mClientManager.getIgnoreFragmentsForSession(session);
    }

    @VisibleForTesting
    void setShouldSpeculateLoadOnCellularForSession(SessionHolder<?> session, boolean value) {
        mClientManager.setSpeculateLoadOnCellularForSession(session, value);
    }

    @VisibleForTesting
    public void setCanUseHiddenTabForSession(SessionHolder<?> session, boolean value) {
        mClientManager.setCanUseHiddenTab(session, value);
    }

    /** See {@link ClientManager#setSendNavigationInfoForSession(SessionHolder, boolean)}. */
    void setSendNavigationInfoForSession(@Nullable SessionHolder<?> session, boolean send) {
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
            SessionHolder<?> session,
            Intent intent,
            Supplier<ProfileProvider> profileProviderSupplier) {}

    /**
     * Returns whether the app launching the CCT may display account mismatch notification UI.
     *
     * @param intent The intent that launched the custom tab.
     */
    boolean isAppForAccountMismatchNotification(Intent intent) {
        return false;
    }

    /**
     * Whether the account mismatch notification UI should be shown.
     *
     * @param intent The intent that launched the custom tab.
     * @param profile The current profile object.
     * @param accountId Account ID to be used to access notification data.
     * @param lastShownTime The last time the notification was shown to user.
     * @param mimData Mismatch notification data.
     * @return Whether the notification was shown or not.
     */
    boolean shouldShowAccountMismatchNotification(
            Intent intent,
            Profile profile,
            String accountId,
            long lastShownTime,
            MismatchNotificationData mimData) {
        return false;
    }

    /**
     * Show the name of the account in which the app launching the custom tab is signed.
     *
     * @param intent The intent that launched the custom tab.
     */
    @Nullable String getAppAccountName(Intent intent) {
        return null;
    }

    /**
     * Sends a callback using {@link CustomTabsCallback} with the first run result if necessary.
     *
     * @param intentExtras The extras for the initial VIEW intent that initiated first run.
     * @param resultOK Whether first run was successful.
     */
    public void sendFirstRunCallbackIfNecessary(@Nullable Bundle intentExtras, boolean resultOK) {}

    /**
     * Sends the navigation info that was captured to the client callback.
     *
     * @param session The session to use for getting client callback.
     * @param url The current url for the tab.
     * @param title The current title for the tab.
     * @param snapshotPath Uri location for screenshot of the tab contents which is publicly
     *     available for sharing.
     */
    public void sendNavigationInfo(
            @Nullable SessionHolder<?> session,
            String url,
            String title,
            @Nullable Uri snapshotPath) {}

    /**
     * Called when the bottom bar for the custom tab has been hidden or shown completely by user
     * scroll.
     *
     * @param session The session that is linked with the custom tab.
     * @param hidden Whether the bottom bar is hidden or shown.
     */
    public void onBottomBarScrollStateChanged(@Nullable SessionHolder<?> session, boolean hidden) {
        Bundle args = new Bundle();
        args.putBoolean("hidden", hidden);

        if (safeExtraCallback(session, BOTTOM_BAR_SCROLL_STATE_CALLBACK, args) && mLogRequests) {
            logCallback("extraCallback(" + BOTTOM_BAR_SCROLL_STATE_CALLBACK + ")", hidden);
        }
    }

    /** Called when a resizable Custom Tab is resized. */
    public void onResized(@Nullable SessionHolder<?> session, int height, int width) {
        Bundle args = new Bundle();
        if (height != mPrevHeight) {
            args.putInt(ON_RESIZED_SIZE_EXTRA, height);

            // TODO(crbug.com/40867201): Deprecate the extra callback.
            if (safeExtraCallback(session, ON_RESIZED_CALLBACK, args) && mLogRequests) {
                logCallback("extraCallback(" + ON_RESIZED_CALLBACK + ")", args);
            }
            mPrevHeight = height;
        }

        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
    public void onUnminimized(@Nullable SessionHolder<?> session) {
        Bundle args = new Bundle();

        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
    public void onMinimized(@Nullable SessionHolder<?> session) {
        Bundle args = new Bundle();

        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
            @Nullable SessionHolder<?> session,
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

        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
     * @see {@link notifyNavigationEvent(SessionHolder, int, int)}
     */
    public boolean notifyNavigationEvent(@Nullable SessionHolder<?> session, int navigationEvent) {
        return notifyNavigationEvent(session, navigationEvent, /* errorCode= */ null);
    }

    /**
     * Notifies the application of a navigation event.
     *
     * <p>Delivers the {@link CustomTabsCallback#onNavigationEvent} callback to the application.
     *
     * @param session The Binder object identifying the session.
     * @param navigationEvent The navigation event code, defined in {@link CustomTabsCallback}
     * @param errorCode Network error code. Null if there was no error or the error code is not in
     *     the list of error codes that should be passed to the embedder.
     * @return true for success.
     */
    public boolean notifyNavigationEvent(
            @Nullable SessionHolder<?> session, int navigationEvent, @Nullable Integer errorCode) {
        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
        if (callback == null) return false;
        try {
            Bundle extra = getExtrasBundleForNavigationEventForSession(session);
            if (errorCode != null) extra.putInt("navigationEventErrorCode", errorCode);
            callback.onNavigationEvent(navigationEvent, extra);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
            return false;
        }
        logCallback("onNavigationEvent()", navigationEvent);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.NavigationEvent",
                navigationEvent,
                CustomTabsNavigationEvent.NUM_ENTRIES);
        return true;
    }

    /**
     * @return The {@link Bundle} to use as extra to {@link
     *     CustomTabsCallback#onNavigationEvent(int, Bundle)}
     */
    protected Bundle getExtrasBundleForNavigationEventForSession(
            @Nullable SessionHolder<?> session) {
        // SystemClock.uptimeMillis() is used here as it (as of June 2017) uses the same system call
        // as all the native side of Chrome, and this is the same clock used for page load metrics.
        Bundle extras = new Bundle();
        extras.putLong("timestampUptimeMillis", SystemClock.uptimeMillis());
        return extras;
    }

    private void notifyWarmupIsDone(int uid, @Nullable Runnable internalCallback) {
        ThreadUtils.assertOnUiThread();
        final Bundle args = new Bundle(); // Empty one - safe to reuse for all the callbacks.

        // Notifies all the sessions, as warmup() is tied to a UID, not a session.
        for (SessionHolder<?> session : mClientManager.uidToSessions(uid)) {
            // TODO(crbug.com/40932858): Remove extra callback after its usage dwindles down.
            safeExtraCallback(session, ON_WARMUP_COMPLETED, null);

            BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
            if (callback == null) continue;
            try {
                callback.onWarmupCompleted(args);
            } catch (Exception e) {
                // Catching all exceptions is really bad, but we need it here,
                // because Android exposes us to client bugs by throwing a variety
                // of exceptions. See crbug.com/517023.
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
     * @param navigationStartMicros Absolute navigation start time, in microseconds, in {@link
     *     SystemClock#uptimeMillis()} timebase.
     * @param offsetMs Offset in ms from navigationStart for the page load metric.
     * @return Whether the metric has been dispatched to the client.
     */
    boolean notifySinglePageLoadMetric(
            SessionHolder<?> session,
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
     * <p>TODD(lizeb): Move this to a proper method in {@link CustomTabsCallback} once one is
     * available.
     *
     * @param session Session identifier.
     * @param args Bundle containing metric information to update. Each item in the bundle should be
     *     a key specifying the metric name and the metric value as the value.
     */
    boolean notifyPageLoadMetrics(SessionHolder<?> session, Bundle args) {
        if (!mClientManager.shouldGetPageLoadMetrics(session)) return false;
        if (safeExtraCallback(session, PAGE_LOAD_METRICS_CALLBACK, args)) {
            logPageLoadMetricsCallback(args);
            return true;
        }
        return false;
    }

    /**
     * Notifies the application and {@link EngagementSignalsHandler} that the user has selected to
     * open the page in their browser. This method should be called before initiating the transfer.
     *
     * @param session Session identifier.
     * @param tab the tab being taken out of CCT.
     * @return true if application was successfully notified. To protect Chrome exceptions in the
     *     client application are swallowed and false is returned.
     */
    public boolean notifyOpenInBrowser(SessionHolder<?> session, Tab tab) {
        EngagementSignalsHandler engagementSignalsHandler = getEngagementSignalsHandler(session);
        if (tab != null && engagementSignalsHandler != null) {
            engagementSignalsHandler.notifyOpenInBrowser(tab);
        }
        // Reset the client data header for the WebContents since it's not a CCT tab anymore.
        if (tab != null && tab.getWebContents() != null) {
            CustomTabsConnectionJni.get().setClientDataHeader(tab.getWebContents(), "");
        }
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
            @Nullable SessionHolder<?> session, String callbackName, @Nullable Bundle args) {
        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
     * Calls {@link CustomTabsCallback#extraCallbackWithResult)}. Wraps calling
     * sendExtraCallbackWithResult in a try/catch so that exceptions thrown by the host app don't
     * crash Chrome.
     */
    @Nullable // The string passed is safe since it is a method name.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    public Bundle sendExtraCallbackWithResult(
            @Nullable SessionHolder session, String callbackName, @Nullable Bundle args) {
        BrowserCallbackWrapper callback = mClientManager.getCallbackForSession(session);
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
     * <p>The application is kept alive (that is, raised to at least the current process priority
     * level) until {@link #dontKeepAliveForSession} is called.
     *
     * @param session The Binder object identifying the session.
     * @param intent Intent describing the service to bind to.
     * @return true for success.
     */
    boolean keepAliveForSession(@Nullable SessionHolder<?> session, @Nullable Intent intent) {
        return mClientManager.keepAliveForSession(session, intent);
    }

    /**
     * Lets the lifetime of the process linked to a given sessionId be managed normally.
     *
     * <p>Without a matching call to {@link #keepAliveForSession}, this is a no-op.
     *
     * @param session The Binder object identifying the session.
     */
    void dontKeepAliveForSession(@Nullable SessionHolder<?> session) {
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
    static @Nullable String getSchedulerGroup(int pid) {
        // Android uses several cgroups for processes, depending on their priority. The list of
        // cgroups a process is part of can be queried by reading /proc/<pid>/cgroup, which is
        // world-readable.
        String cgroupFilename = "/proc/" + pid + "/cgroup";
        String controllerName = "cpuset";
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
     *
     * @param session The session that has been destroyed.
     */
    @VisibleForTesting
    void cleanUpSession(final CustomTabsSessionToken session) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mClientManager.cleanupSession(new SessionHolder<>(session)));
    }

    /**
     * Discards substantial objects that are not currently in use.
     *
     * @param level The type of signal as defined in {@link ComponentCallbacks2}.
     */
    public static void onTrimMemory(int level) {
        if (!hasInstance()) return;

        if (ChromeApplicationImpl.isSevereMemorySignal(level)) {
            getInstance().mClientManager.cleanupUnusedSessions();
        }
    }

    boolean maySpeculate(SessionHolder<?> session) {
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
    public void cancelSpeculation(@Nullable SessionHolder<?> session) {
        ThreadUtils.assertOnUiThread();
        mHiddenTabHolder.destroyHiddenTab(session);
    }

    /*
     * This function will do as much as it can to have a subsequent navigation
     * to the specified url sped up, including speculatively loading a url, preconnecting,
     * and starting a spare renderer.
     */
    private void startSpeculation(
            SessionHolder<?> session,
            String url,
            boolean useHiddenTab,
            @Nullable Bundle extras,
            boolean useSeparateStoragePartitionForExperiment) {
        WarmupManager warmupManager = WarmupManager.getInstance();
        Profile profile = ProfileManager.getLastUsedRegularProfile();

        // At most one on-going speculation, clears the previous one.
        cancelSpeculation(null);

        if (useHiddenTab) {
            launchUrlInHiddenTab(
                    session, profile, url, extras, useSeparateStoragePartitionForExperiment);
        } else {
            createSpareTab(profile);
        }
        warmupManager.maybePreconnectUrlAndSubResources(profile, url);
    }

    /** Creates a hidden tab and initiates a navigation. */
    private void launchUrlInHiddenTab(
            SessionHolder<?> session,
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
    public @Nullable Referrer getDefaultReferrerForSession(SessionHolder<?> session) {
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
            CustomTabsSessionToken session, @Nullable Supplier<Boolean> supplier) {
        mClientManager.setEngagementSignalsAvailableSupplierForSession(
                new SessionHolder<>(session), supplier);
    }

    public @Nullable EngagementSignalsHandler getEngagementSignalsHandler(
            @Nullable SessionHolder<?> session) {
        return mClientManager.getEngagementSignalsHandlerForSession(session);
    }

    @CalledByNative
    public static void notifyClientOfDetachedRequestCompletion(
            SessionHolder<?> session, @JniType("std::string") String url, int status) {
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
    HiddenTabHolder.@Nullable SpeculationParams getSpeculationParamsForTesting() {
        return mHiddenTabHolder.getSpeculationParamsForTesting();
    }

    public static void createSpareTab(Profile profile) {
        if (sSkipTabPrewarmingForTesting) return;
        if (SysUtils.isLowEndDevice()) return;
        WarmupManager.getInstance().createRegularSpareTab(profile);
    }

    public boolean receiveFile(
            CustomTabsSessionToken sessionToken, Uri uri, int purpose, @Nullable Bundle extras) {
        return CustomTabsClientFileProcessor.getInstance()
                .processFile(new SessionHolder<>(sessionToken), uri, purpose, extras);
    }

    public void setCustomTabIsInForeground(
            @Nullable SessionHolder<?> session, boolean isInForeground) {
        mClientManager.setCustomTabIsInForeground(session, isInForeground);
    }

    public boolean isEngagementSignalsApiAvailable(
            CustomTabsSessionToken sessionToken, Bundle extras) {
        return isEngagementSignalsApiAvailableInternal(new SessionHolder<>(sessionToken));
    }

    public boolean setEngagementSignalsCallback(
            CustomTabsSessionToken sessionToken,
            EngagementSignalsCallback callback,
            Bundle extras) {
        var session = new SessionHolder<>(sessionToken);
        if (!isEngagementSignalsApiAvailableInternal(session)) return false;

        var engagementSignalsHandler =
                mClientManager.getEngagementSignalsHandlerForSession(session);
        if (engagementSignalsHandler == null) return false;

        mClientManager.setEngagementSignalsCallbackForSession(session, callback);
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> engagementSignalsHandler.setEngagementSignalsCallback(callback));
        return true;
    }

    private boolean isEngagementSignalsApiAvailableInternal(SessionHolder<?> session) {
        var supplier = mClientManager.getEngagementSignalsAvailableSupplierForSession(session);
        return supplier != null
                ? supplier.get()
                : PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    public boolean hasEngagementSignalsCallback(SessionHolder<?> session) {
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
    // TODO(crbug.com/422969546): Remove this method once the new method is used.

    public @Nullable Consumer<Tab> getAlternateOmniboxTapHandler(
            BrowserServicesIntentDataProvider intentData) {
        return null;
    }

    /**
     * Returns an alternate handler for taps on the Custom Tabs Omnibox. The function returns true
     * if the tap was handled, false otherwise.
     */
    // TODO(crbug.com/422969546): Rename to getAlternateOmniboxTapHandler once the old method is
    // removed.
    public Function<Tab, Boolean> getAlternateOmniboxTapHandlerWithVerification(
            BrowserServicesIntentDataProvider intentData) {
        return (tab) -> false;
    }

    /** Specifies what content should be presented by the CustomTabs instance in location bar. */
    public @TitleVisibility int getTitleVisibilityState(
            BrowserServicesIntentDataProvider intentData) {
        if (shouldEnableOmniboxForIntent(intentData)) {
            return CustomTabIntentDataProvider.TitleVisibility.HIDDEN;
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
            SessionHolder<?> session,
            @JniType("std::string") String stateKey,
            String[] foundTextFragments) {
        getInstance()
                .notifyClientOfTextFragmentLookupCompletionReportApp(
                        session, stateKey, new ArrayList(Arrays.asList(foundTextFragments)));
    }

    protected void notifyClientOfTextFragmentLookupCompletionReportApp(
            SessionHolder<?> session, String stateKey, ArrayList<String> foundTextFragments) {}

    /**
     * @return The CalledWarmup state for the session.
     */
    public @CalledWarmup int getWarmupState(@Nullable SessionHolder<?> session) {
        return mClientManager.getWarmupState(session);
    }

    /** Kicks off a navigation in the background before the CustomTabActivity is started. */
    public boolean startEarlyNavigationInHiddenTab(Profile profile, Intent intent) {
        return mHiddenTabHolder.startEarlynavigation(profile, intent);
    }

    void cleanUpSession(AuthTabSessionToken session) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mClientManager.cleanupSession(new SessionHolder<>(session)));
    }

    public boolean newAuthTabSession(AuthTabSessionToken session) {
        SessionHolder<AuthTabSessionToken> holder = new SessionHolder<>(session);
        boolean success = newSessionInternal(holder);
        logCall("newSession()", success);
        return success;
    }

    public boolean isSessionValid(SessionHolder<?> session) {
        return mClientManager.isSessionValid(session);
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
                @Nullable SessionHolder<?> session,
                @JniType("std::string") @Nullable String packageName,
                @JniType("std::string") String url,
                @JniType("std::string") String origin,
                int referrerPolicy,
                @DetachedResourceRequestMotivation int motivation);

        void setClientDataHeader(WebContents webContents, @JniType("std::string") String header);

        void textFragmentLookup(
                SessionHolder<?> session,
                WebContents webContents,
                @JniType("std::string") String stateKey,
                String[] textFragment);

        void textFragmentFindScrollAndHighlight(
                SessionHolder<?> session,
                WebContents webContents,
                @JniType("std::string") String textFragment);
    }
}
