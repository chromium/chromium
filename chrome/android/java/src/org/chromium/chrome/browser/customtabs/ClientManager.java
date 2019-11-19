// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.SparseBooleanArray;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsService.Relation;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PostMessageServiceConnection;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.browserservices.OriginVerifier.OriginVerificationListener;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.installedapp.InstalledAppProviderImpl;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Manages the clients' state for Custom Tabs. This class is threadsafe. */
class ClientManager {
    // Values for the "CustomTabs.MayLaunchUrlType" UMA histogram. Append-only.
    @IntDef({MayLaunchUrlType.NO_MAY_LAUNCH_URL, MayLaunchUrlType.LOW_CONFIDENCE,
            MayLaunchUrlType.HIGH_CONFIDENCE, MayLaunchUrlType.BOTH})
    @Retention(RetentionPolicy.SOURCE)
    @interface MayLaunchUrlType {
        @VisibleForTesting
        int NO_MAY_LAUNCH_URL = 0;
        @VisibleForTesting
        int LOW_CONFIDENCE = 1;
        @VisibleForTesting
        int HIGH_CONFIDENCE = 2;
        @VisibleForTesting
        int BOTH = 3; // LOW + HIGH.
        int NUM_ENTRIES = 4;
    }

    // Values for the "CustomTabs.PredictionStatus" UMA histogram. Append-only.
    @IntDef({PredictionStatus.NONE, PredictionStatus.GOOD, PredictionStatus.BAD})
    @Retention(RetentionPolicy.SOURCE)
    @interface PredictionStatus {
        @VisibleForTesting
        int NONE = 0;
        @VisibleForTesting
        int GOOD = 1;
        @VisibleForTesting
        int BAD = 2;
        int NUM_ENTRIES = 3;
    }

    // Values for the "CustomTabs.CalledWarmup" UMA histogram. Append-only.
    @IntDef({CalledWarmup.NO_SESSION_NO_WARMUP, CalledWarmup.NO_SESSION_WARMUP,
            CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED,
            CalledWarmup.SESSION_NO_WARMUP_NOT_CALLED, CalledWarmup.SESSION_WARMUP})
    @Retention(RetentionPolicy.SOURCE)
    @interface CalledWarmup {
        @VisibleForTesting
        int NO_SESSION_NO_WARMUP = 0;
        @VisibleForTesting
        int NO_SESSION_WARMUP = 1;
        @VisibleForTesting
        int SESSION_NO_WARMUP_ALREADY_CALLED = 2;
        @VisibleForTesting
        int SESSION_NO_WARMUP_NOT_CALLED = 3;
        @VisibleForTesting
        int SESSION_WARMUP = 4;
        @VisibleForTesting
        int NUM_ENTRIES = 5;
    }

    /** To be called when a client gets disconnected. */
    public interface DisconnectCallback { public void run(CustomTabsSessionToken session); }

    private static class KeepAliveServiceConnection implements ServiceConnection {
        private final Context mContext;
        private final Intent mServiceIntent;
        private boolean mHasDied;
        private boolean mIsBound;

        public KeepAliveServiceConnection(Context context, Intent serviceIntent) {
            mContext = context;
            mServiceIntent = serviceIntent;
        }

        /**
         * Connects to the service identified by |serviceIntent|. Does not reconnect if the service
         * got disconnected at some point from the other end (remote process death).
         */
        public boolean connect() {
            if (mIsBound) return true;
            // If the remote process died at some point, it doesn't make sense to resurrect it.
            if (mHasDied) return false;

            boolean ok;
            try {
                ok = mContext.bindService(mServiceIntent, this, Context.BIND_AUTO_CREATE);
            } catch (SecurityException e) {
                return false;
            }
            mIsBound = ok;
            return ok;
        }

        /**
         * Disconnects from the remote process. Safe to call even if {@link #connect} returned
         * false, or if the remote service died.
         */
        public void disconnect() {
            if (mIsBound) {
                mContext.unbindService(this);
                mIsBound = false;
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {}

        @Override
        public void onServiceDisconnected(ComponentName name) {
            if (mIsBound) {
                // The remote process has died. This typically happens if the system is low enough
                // on memory to kill one of the last process on the "kill list". In this case, we
                // shouldn't resurrect the process (which happens with BIND_AUTO_CREATE) because
                // that could create a "restart/kill" loop.
                mHasDied = true;
                disconnect();
            }
        }
    }

    /** Per-session values. */
    private static class SessionParams {
        public final int uid;
        private CustomTabsCallback mCustomTabsCallback;
        public final DisconnectCallback disconnectCallback;
        public final PostMessageHandler postMessageHandler;
        public final PostMessageServiceConnection serviceConnection;
        public final Set<Origin> mLinkedOrigins = new HashSet<>();
        public OriginVerifier originVerifier;
        public boolean mIgnoreFragments;
        public boolean lowConfidencePrediction;
        public boolean highConfidencePrediction;
        private String mPackageName;
        private boolean mShouldHideDomain;
        private boolean mShouldSpeculateLoadOnCellular;
        private boolean mShouldSendNavigationInfo;
        private boolean mShouldSendBottomBarScrollState;
        private KeepAliveServiceConnection mKeepAliveConnection;
        private String mPredictedUrl;
        private long mLastMayLaunchUrlTimestamp;
        private boolean mCanUseHiddenTab;
        private boolean mAllowParallelRequest;
        private boolean mAllowResourcePrefetch;
        private boolean mShouldGetPageLoadMetrics;
        private boolean mShouldHideTopBar;

        public SessionParams(Context context, int uid, CustomTabsCallback customTabsCallback,
                DisconnectCallback callback, PostMessageHandler postMessageHandler,
                PostMessageServiceConnection serviceConnection) {
            this.uid = uid;
            mPackageName = getPackageName(context, uid);
            mCustomTabsCallback = customTabsCallback;
            disconnectCallback = callback;
            this.postMessageHandler = postMessageHandler;
            this.serviceConnection = serviceConnection;
            if (postMessageHandler != null) this.serviceConnection.setPackageName(mPackageName);
        }

        /**
         * Overrides package name with given String. TO be used for testing only.
         */
        void overridePackageNameForTesting(String newPackageName) {
            mPackageName = newPackageName;
        }

        /**
         * @return The package name for this session.
         */
        public String getPackageName() {
            return mPackageName;
        }

        private static String getPackageName(Context context, int uid) {
            PackageManager packageManager = context.getPackageManager();
            String[] packageList = packageManager.getPackagesForUid(uid);
            if (packageList.length != 1 || TextUtils.isEmpty(packageList[0])) return null;
            return packageList[0];
        }

        public KeepAliveServiceConnection getKeepAliveConnection() {
            return mKeepAliveConnection;
        }

        public void setKeepAliveConnection(KeepAliveServiceConnection serviceConnection) {
            mKeepAliveConnection = serviceConnection;
        }

        public void setPredictionMetrics(
                String predictedUrl, long lastMayLaunchUrlTimestamp, boolean lowConfidence) {
            mPredictedUrl = predictedUrl;
            mLastMayLaunchUrlTimestamp = lastMayLaunchUrlTimestamp;
            highConfidencePrediction |= !TextUtils.isEmpty(predictedUrl);
            lowConfidencePrediction |= lowConfidence;
        }

        /**
         * Resets the prediction metrics. This clears the predicted URL, last prediction time,
         * and whether a low and/or high confidence prediction has been done.
         */
        public void resetPredictionMetrics() {
            mPredictedUrl = null;
            mLastMayLaunchUrlTimestamp = 0;
            highConfidencePrediction = false;
            lowConfidencePrediction = false;
        }

        public String getPredictedUrl() {
            return mPredictedUrl;
        }

        public long getLastMayLaunchUrlTimestamp() {
            return mLastMayLaunchUrlTimestamp;
        }

        /**
         * @return Whether the default parameters are used for this session.
         */
        public boolean isDefault() {
            return !mIgnoreFragments && !mShouldSpeculateLoadOnCellular;
        }

        public CustomTabsCallback getCustomTabsCallback() {
            return mCustomTabsCallback;
        }

        public void setCustomTabsCallback(CustomTabsCallback customTabsCallback) {
            mCustomTabsCallback = customTabsCallback;
        }
    }

    private final Map<CustomTabsSessionToken, SessionParams> mSessionParams = new HashMap<>();

    private final SparseBooleanArray mUidHasCalledWarmup = new SparseBooleanArray();
    private boolean mWarmupHasBeenCalled;

    public ClientManager() {
        RequestThrottler.loadInBackground();
    }

    /** Creates a new session.
     *
     * @param session Session provided by the client.
     * @param uid Client UID, as returned by Binder.getCallingUid(),
     * @param onDisconnect To be called on the UI thread when a client gets disconnected.
     * @param postMessageHandler The handler to be used for postMessage related operations.
     * @return true for success.
     */
    public synchronized boolean newSession(CustomTabsSessionToken session, int uid,
            DisconnectCallback onDisconnect, @NonNull PostMessageHandler postMessageHandler,
            @NonNull PostMessageServiceConnection serviceConnection) {
        if (session == null || session.getCallback() == null) return false;
        if (mSessionParams.containsKey(session)) {
            mSessionParams.get(session).setCustomTabsCallback(session.getCallback());
        } else {
            SessionParams params = new SessionParams(ContextUtils.getApplicationContext(), uid,
                    session.getCallback(), onDisconnect, postMessageHandler, serviceConnection);
            mSessionParams.put(session, params);
        }

        return true;
    }

    public synchronized int postMessage(CustomTabsSessionToken session, String message) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        return params.postMessageHandler.postMessageFromClientApp(message);
    }

    /**
     * Records that {@link CustomTabsConnection#warmup(long)} has been called from the given uid.
     */
    public synchronized void recordUidHasCalledWarmup(int uid) {
        mWarmupHasBeenCalled = true;
        mUidHasCalledWarmup.put(uid, true);
    }

    /**
     * @return all the sessions originating from a given {@code uid}.
     */
    public synchronized List<CustomTabsSessionToken> uidToSessions(int uid) {
        List<CustomTabsSessionToken> sessions = new ArrayList<>();
        for (Map.Entry<CustomTabsSessionToken, SessionParams> entry : mSessionParams.entrySet()) {
            if (entry.getValue().uid == uid) sessions.add(entry.getKey());
        }
        return sessions;
    }

    /** Updates the client behavior stats and returns whether speculation is allowed.
     *
     * The first call to the "low priority" mode is not throttled. Subsequent ones are.
     *
     * @param session Client session.
     * @param uid As returned by Binder.getCallingUid().
     * @param url Predicted URL.
     * @param lowConfidence whether the request contains some "low confidence" URLs.
     * @return true if speculation is allowed.
     */
    public synchronized boolean updateStatsAndReturnWhetherAllowed(
            CustomTabsSessionToken session, int uid, String url, boolean lowConfidence) {
        SessionParams params = mSessionParams.get(session);
        if (params == null || params.uid != uid) return false;
        boolean firstLowConfidencePrediction =
                TextUtils.isEmpty(url) && lowConfidence && !params.lowConfidencePrediction;
        params.setPredictionMetrics(url, SystemClock.elapsedRealtime(), lowConfidence);
        if (firstLowConfidencePrediction) return true;
        RequestThrottler throttler = RequestThrottler.getForUid(uid);
        return throttler.updateStatsAndReturnWhetherAllowed();
    }

    @VisibleForTesting
    synchronized @CalledWarmup int getWarmupState(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        boolean hasValidSession = params != null;
        boolean hasUidCalledWarmup = hasValidSession && mUidHasCalledWarmup.get(params.uid);
        int result = mWarmupHasBeenCalled ? CalledWarmup.NO_SESSION_WARMUP
                                          : CalledWarmup.NO_SESSION_NO_WARMUP;
        if (hasValidSession) {
            if (hasUidCalledWarmup) {
                result = CalledWarmup.SESSION_WARMUP;
            } else {
                result = mWarmupHasBeenCalled ? CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED
                                              : CalledWarmup.SESSION_NO_WARMUP_NOT_CALLED;
            }
        }
        return result;
    }

    /**
     * @return the prediction outcome. PredictionStatus.NONE if mSessionParams.get(session) returns
     * null.
     */
    @VisibleForTesting
    synchronized @PredictionStatus int getPredictionOutcome(
            CustomTabsSessionToken session, String url) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return PredictionStatus.NONE;

        String predictedUrl = params.getPredictedUrl();
        if (predictedUrl == null) return PredictionStatus.NONE;

        boolean urlsMatch = TextUtils.equals(predictedUrl, url)
                || (params.mIgnoreFragments
                        && UrlUtilities.urlsMatchIgnoringFragments(predictedUrl, url));
        return urlsMatch ? PredictionStatus.GOOD : PredictionStatus.BAD;
    }

    /**
     * Registers that a client has launched a URL inside a Custom Tab.
     */
    public synchronized void registerLaunch(CustomTabsSessionToken session, String url) {
        @PredictionStatus
        int outcome = getPredictionOutcome(session, url);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.PredictionStatus", outcome, PredictionStatus.NUM_ENTRIES);

        SessionParams params = mSessionParams.get(session);
        if (outcome == PredictionStatus.GOOD) {
            long elapsedTimeMs = SystemClock.elapsedRealtime()
                    - params.getLastMayLaunchUrlTimestamp();
            RequestThrottler.getForUid(params.uid).registerSuccess(params.mPredictedUrl);
            RecordHistogram.recordCustomTimesHistogram("CustomTabs.PredictionToLaunch",
                    elapsedTimeMs, 1, DateUtils.MINUTE_IN_MILLIS * 3, 100);
        }
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.WarmupStateOnLaunch",
                getWarmupState(session), CalledWarmup.NUM_ENTRIES);

        if (params == null) return;

        @MayLaunchUrlType
        int value = (params.lowConfidencePrediction ? MayLaunchUrlType.LOW_CONFIDENCE : 0)
                + (params.highConfidencePrediction ? MayLaunchUrlType.HIGH_CONFIDENCE : 0);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.MayLaunchUrlType", value, MayLaunchUrlType.NUM_ENTRIES);
        params.resetPredictionMetrics();
    }

    /**
     * See {@link PostMessageServiceConnection#bindSessionToPostMessageService(Context, String)}.
     */
    public synchronized boolean bindToPostMessageServiceForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return false;
        return params.serviceConnection.bindSessionToPostMessageService(
                ContextUtils.getApplicationContext());
    }

    /**
     * See {@link PostMessageHandler#initializeWithPostMessageUri(Uri)}.
     */
    public synchronized void initializeWithPostMessageOriginForSession(
            CustomTabsSessionToken session, Uri origin) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return;
        params.postMessageHandler.initializeWithPostMessageUri(origin);
    }

    public synchronized boolean validateRelationship(
            CustomTabsSessionToken session, int relation, Origin origin, Bundle extras) {
        return validateRelationshipInternal(session, relation, origin, false);
    }

    /**
     * Validates the link between the client and the origin.
     */
    public synchronized void verifyAndInitializeWithPostMessageOriginForSession(
            CustomTabsSessionToken session, Origin origin, @Relation int relation) {
        validateRelationshipInternal(session, relation, origin, true);
    }

    /**
     * Can't be called on UI Thread.
     */
    private synchronized boolean validateRelationshipInternal(CustomTabsSessionToken session,
            int relation, Origin origin, boolean initializePostMessageChannel) {
        SessionParams params = mSessionParams.get(session);
        if (params == null || TextUtils.isEmpty(params.getPackageName())) return false;

        OriginVerificationListener listener = (packageName, verifiedOrigin, verified, online) -> {
            assert origin.equals(verifiedOrigin);

            CustomTabsCallback callback = getCallbackForSession(session);
            if (callback != null) {
                Bundle extras = null;
                if (verified && online != null) {
                    extras = new Bundle();
                    extras.putBoolean(CustomTabsCallback.ONLINE_EXTRAS_KEY, online);
                }
                callback.onRelationshipValidationResult(relation, origin.uri(), verified, extras);
            }
            if (initializePostMessageChannel) {
                params.postMessageHandler
                        .onOriginVerified(packageName, verifiedOrigin, verified, online);
            }
        };

        params.originVerifier =
                new OriginVerifier(params.getPackageName(), relation, /* webContents= */ null);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { params.originVerifier.start(listener, origin); });
        if (relation == CustomTabsService.RELATION_HANDLE_ALL_URLS
                && InstalledAppProviderImpl.isAppInstalledAndAssociatedWithOrigin(
                           params.getPackageName(), URI.create(origin.toString()),
                           ContextUtils.getApplicationContext().getPackageManager())) {
            params.mLinkedOrigins.add(origin);
        }
        return true;
    }

    /**
     * @return The postMessage origin for the given session.
     */
    @VisibleForTesting
    synchronized Uri getPostMessageOriginForSessionForTesting(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return null;
        return params.postMessageHandler.getPostMessageUriForTesting();
    }

    /**
     * See {@link PostMessageHandler#reset(WebContents)}.
     */
    public synchronized void resetPostMessageHandlerForSession(
            CustomTabsSessionToken session, WebContents webContents) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return;
        params.postMessageHandler.reset(webContents);
    }

    /**
     * @return The referrer that is associated with the client owning given session.
     */
    public synchronized Referrer getReferrerForSession(CustomTabsSessionToken session) {
        return IntentHandler.constructValidReferrerForAuthority(
                getClientPackageNameForSession(session));
    }

    /**
     * @return The package name associated with the client owning the given session.
     */
    public synchronized String getClientPackageNameForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params == null ? null : params.getPackageName();
    }

    /**
     * Overrides the package name for the given session to be the given package name. To be used
     * for testing only.
     */
    public synchronized void overridePackageNameForSession(
            CustomTabsSessionToken session, String packageName) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.overridePackageNameForTesting(packageName);
    }

    /**
     * @return The callback {@link CustomTabsSessionToken} for the given session.
     */
    public synchronized CustomTabsCallback getCallbackForSession(CustomTabsSessionToken session) {
        if (session != null && mSessionParams.containsKey(session)) {
            return mSessionParams.get(session).getCustomTabsCallback();
        }
        return null;
    }

    /**
     * @return Whether the urlbar should be hidden for the session on first page load. Urls are
     *         foced to show up after the user navigates away.
     */
    public synchronized boolean shouldHideDomainForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mShouldHideDomain : false;
    }

    /**
     * Sets whether the urlbar should be hidden for a given session.
     */
    public synchronized void setHideDomainForSession(CustomTabsSessionToken session, boolean hide) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mShouldHideDomain = hide;
    }

    /**
     * @return Whether bottom bar scrolling state should be recorded and shared for the session.
     */
    public synchronized boolean shouldSendBottomBarScrollStateForSession(
            CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mShouldSendBottomBarScrollState : false;
    }

    /**
     * Sets whether bottom bar scrolling state should be recorded and shared for the session.
     */
    public synchronized void setSendBottomBarScrollingStateForSessionn(
            CustomTabsSessionToken session, boolean send) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mShouldSendBottomBarScrollState = send;
    }

    /**
     * @return Whether navigation info should be recorded and shared for the session.
     */
    public synchronized boolean shouldSendNavigationInfoForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mShouldSendNavigationInfo : false;
    }

    /**
     * Sets whether navigation info should be recorded and shared for the current navigation in this
     * session.
     */
    public synchronized void setSendNavigationInfoForSession(
            CustomTabsSessionToken session, boolean send) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mShouldSendNavigationInfo = send;
    }

    /**
     * @return Whether the fragment should be ignored for speculation matching.
     */
    public synchronized boolean getIgnoreFragmentsForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params == null ? false : params.mIgnoreFragments;
    }

    /** Sets whether the fragment should be ignored for speculation matching. */
    public synchronized void setIgnoreFragmentsForSession(
            CustomTabsSessionToken session, boolean value) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mIgnoreFragments = value;
    }

    /**
     * @return Whether load speculation should be turned on for cellular networks for given session.
     */
    public synchronized boolean shouldSpeculateLoadOnCellularForSession(
            CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mShouldSpeculateLoadOnCellular : false;
    }

    /**
     * @return Whether the CCT TopBar should be hidden on dynamic module managed URLs
     * for a given session.
     */
    public synchronized boolean shouldHideTopBarOnModuleManagedUrlsForSession(
            CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null && params.mShouldHideTopBar;
    }

    /**
     * Sets whether the CCT TopBar should be hidden on dynamic module managed URLs
     * for a given session.
     */
    public synchronized void setHideCCTTopBarOnModuleManagedUrls(
            CustomTabsSessionToken session, boolean hide) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mShouldHideTopBar = hide;
    }

    /**
     * @return Whether the session is using the default parameters (that is, don't ignore
     *         fragments and don't speculate loads on cellular connections).
     */
    public synchronized boolean usesDefaultSessionParameters(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.isDefault() : true;
    }

    /**
     * Sets whether speculation should be turned on for mobile networks for given session.
     * If it is turned on, hidden tab speculation is turned on as well.
     */
    public synchronized void setSpeculateLoadOnCellularForSession(
            CustomTabsSessionToken session, boolean shouldSpeculate) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) {
            params.mShouldSpeculateLoadOnCellular = shouldSpeculate;
            params.mCanUseHiddenTab = shouldSpeculate;
        }
    }

    /**
     * Sets whether hidden tab speculation can be used.
     */
    public synchronized void setCanUseHiddenTab(
            CustomTabsSessionToken session, boolean canUseHiddenTab) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) {
            params.mCanUseHiddenTab = canUseHiddenTab;
        }
    }

    /**
     * Get whether hidden tab speculation can be used. The default is false.
     */
    public synchronized boolean getCanUseHiddenTab(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params == null ? false : params.mCanUseHiddenTab;
    }

    public synchronized void setAllowParallelRequestForSession(
            CustomTabsSessionToken session, boolean allowed) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mAllowParallelRequest = allowed;
    }

    public synchronized boolean getAllowParallelRequestForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mAllowParallelRequest : false;
    }

    public synchronized void setAllowResourcePrefetchForSession(
            CustomTabsSessionToken session, boolean allowed) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mAllowResourcePrefetch = allowed;
    }

    public synchronized boolean getAllowResourcePrefetchForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mAllowResourcePrefetch : false;
    }

    public synchronized void setShouldGetPageLoadMetricsForSession(
            CustomTabsSessionToken session, boolean allowed) {
        SessionParams params = mSessionParams.get(session);
        if (params != null) params.mShouldGetPageLoadMetrics = allowed;
    }

    public synchronized boolean shouldGetPageLoadMetrics(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        return params != null ? params.mShouldGetPageLoadMetrics : false;
    }

    /**
     * Returns whether an origin is first-party with respect to a session, that is if the
     * application linked to the session has a relation with the provided origin. This does not
     * calls OriginVerifier, but only checks the cached relations.
     *
     * @param session The session.
     * @param origin Origin to verify
     */
    public synchronized boolean isFirstPartyOriginForSession(
            CustomTabsSessionToken session, Origin origin) {
        return OriginVerifier.wasPreviouslyVerified(getClientPackageNameForSession(session), origin,
                CustomTabsService.RELATION_USE_AS_ORIGIN);
    }

    /** Tries to bind to a client to keep it alive, and returns true for success. */
    public synchronized boolean keepAliveForSession(CustomTabsSessionToken session, Intent intent) {
        // When an application is bound to a service, its priority is raised to
        // be at least equal to the application's one. This binds to a dummy
        // service (no calls to this service are made).
        if (intent == null || intent.getComponent() == null) return false;
        SessionParams params = mSessionParams.get(session);
        if (params == null) return false;

        KeepAliveServiceConnection connection = params.getKeepAliveConnection();

        if (connection == null) {
            String packageName = intent.getComponent().getPackageName();
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            // Only binds to the application associated to this session.
            if (!Arrays.asList(pm.getPackagesForUid(params.uid)).contains(packageName)) {
                return false;
            }
            Intent serviceIntent = new Intent().setComponent(intent.getComponent());
            connection = new KeepAliveServiceConnection(
                    ContextUtils.getApplicationContext(), serviceIntent);
        }

        boolean ok = connection.connect();
        if (ok) params.setKeepAliveConnection(connection);
        return ok;
    }

    /** Unbind from the KeepAlive service for a client. */
    public synchronized void dontKeepAliveForSession(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        if (params == null || params.getKeepAliveConnection() == null) return;
        KeepAliveServiceConnection connection = params.getKeepAliveConnection();
        connection.disconnect();
    }

    /** See {@link RequestThrottler#isPrerenderingAllowed()} */
    public synchronized boolean isPrerenderingAllowed(int uid) {
        return RequestThrottler.getForUid(uid).isPrerenderingAllowed();
    }

    /** See {@link RequestThrottler#registerPrerenderRequest(String)} */
    public synchronized void registerPrerenderRequest(int uid, String url) {
        RequestThrottler.getForUid(uid).registerPrerenderRequest(url);
    }

    /** See {@link RequestThrottler#reset()} */
    public synchronized void resetThrottling(int uid) {
        RequestThrottler.getForUid(uid).reset();
    }

    /** See {@link RequestThrottler#ban()} */
    public synchronized void ban(int uid) {
        RequestThrottler.getForUid(uid).ban();
    }

    /**
     * Cleans up all data associated with all sessions.
     */
    public synchronized void cleanupAll() {
        // cleanupSessionInternal modifies mSessionParams therefore we need a copy
        List<CustomTabsSessionToken> sessions = new ArrayList<>(mSessionParams.keySet());
        for (CustomTabsSessionToken session : sessions) cleanupSession(session);
    }

    /**
     * Handle any clean up left after a session is destroyed.
     * @param session The session that has been destroyed.
     */
    private synchronized void cleanupSessionInternal(CustomTabsSessionToken session) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return;
        mSessionParams.remove(session);
        if (params.serviceConnection != null) {
            params.serviceConnection.cleanup(ContextUtils.getApplicationContext());
        }
        if (params.originVerifier != null) params.originVerifier.cleanUp();
        if (params.disconnectCallback != null) params.disconnectCallback.run(session);
        mUidHasCalledWarmup.delete(params.uid);
    }

    /**
     * Destroys session when its callback become invalid if the callback is used as identifier.
     *
     * @param session The session with invalid callback.
     */
    public synchronized void cleanupSession(CustomTabsSessionToken session) {
        if (session.hasId()) {
            // Leave session parameters, so client might update callback later.
            // The session will be completely removed when system runs low on memory.
            // {@see #cleanupUnusedSessions}
            mSessionParams.get(session).setCustomTabsCallback(null);
        } else {
            cleanupSessionInternal(session);
        }
    }

    /**
     * Clean up all sessions which are not currently used.
     */
    public synchronized void cleanupUnusedSessions() {
        // cleanupSessionInternal modifies mSessionParams therefore we need a copy
        List<CustomTabsSessionToken> sessions = new ArrayList<>(mSessionParams.keySet());
        for (CustomTabsSessionToken session : sessions) {
            if (mSessionParams.get(session).getCustomTabsCallback() == null) {
                cleanupSessionInternal(session);
            }
        }
    }
}
