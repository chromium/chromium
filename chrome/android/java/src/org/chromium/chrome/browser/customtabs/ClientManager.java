// Copyright 2015 The Chromium Authors
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
import android.util.SparseBooleanArray;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsService.Relation;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.PostMessageServiceConnection;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactoryImpl;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.installedapp.InstalledAppProviderImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
    @IntDef({
        MayLaunchUrlType.NO_MAY_LAUNCH_URL,
        MayLaunchUrlType.LOW_CONFIDENCE,
        MayLaunchUrlType.HIGH_CONFIDENCE,
        MayLaunchUrlType.BOTH,
        MayLaunchUrlType.INVALID_SESSION,
        MayLaunchUrlType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface MayLaunchUrlType {
        @VisibleForTesting int NO_MAY_LAUNCH_URL = 0;
        @VisibleForTesting int LOW_CONFIDENCE = 1;
        @VisibleForTesting int HIGH_CONFIDENCE = 2;
        @VisibleForTesting int BOTH = 3; // LOW + HIGH.
        int INVALID_SESSION = 4;
        int NUM_ENTRIES = 5;
    }

    // Values for the PredictionStatus. Append-only.
    @IntDef({PredictionStatus.NONE, PredictionStatus.GOOD, PredictionStatus.BAD})
    @Retention(RetentionPolicy.SOURCE)
    @interface PredictionStatus {
        @VisibleForTesting int NONE = 0;
        @VisibleForTesting int GOOD = 1;
        @VisibleForTesting int BAD = 2;
        int NUM_ENTRIES = 3;
    }

    // Values for the "CustomTabs.CalledWarmup" UMA histogram. Append-only.
    @IntDef({
        CalledWarmup.NO_SESSION_NO_WARMUP,
        CalledWarmup.NO_SESSION_WARMUP,
        CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED,
        CalledWarmup.SESSION_NO_WARMUP_NOT_CALLED,
        CalledWarmup.SESSION_WARMUP
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CalledWarmup {
        @VisibleForTesting int NO_SESSION_NO_WARMUP = 0;
        @VisibleForTesting int NO_SESSION_WARMUP = 1;
        @VisibleForTesting int SESSION_NO_WARMUP_ALREADY_CALLED = 2;
        @VisibleForTesting int SESSION_NO_WARMUP_NOT_CALLED = 3;
        @VisibleForTesting int SESSION_WARMUP = 4;
        @VisibleForTesting int NUM_ENTRIES = 5;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Values for the "CustomTabs.SessionDisconnectStatus" UMA histogram. Append-only.
    @IntDef({
        SessionDisconnectStatus.UNKNOWN,
        SessionDisconnectStatus.CT_FOREGROUND,
        SessionDisconnectStatus.CT_FOREGROUND_KEEP_ALIVE,
        SessionDisconnectStatus.CT_BACKGROUND,
        SessionDisconnectStatus.CT_BACKGROUND_KEEP_ALIVE,
        SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND,
        SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND_KEEP_ALIVE,
        SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND,
        SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND_KEEP_ALIVE,
        SessionDisconnectStatus.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SessionDisconnectStatus {
        @VisibleForTesting int UNKNOWN = 0;
        @VisibleForTesting int CT_FOREGROUND = 1;
        @VisibleForTesting int CT_FOREGROUND_KEEP_ALIVE = 2;
        @VisibleForTesting int CT_BACKGROUND = 3;
        @VisibleForTesting int CT_BACKGROUND_KEEP_ALIVE = 4;
        @VisibleForTesting int LOW_MEMORY_CT_FOREGROUND = 5;
        @VisibleForTesting int LOW_MEMORY_CT_FOREGROUND_KEEP_ALIVE = 6;
        @VisibleForTesting int LOW_MEMORY_CT_BACKGROUND = 7;
        @VisibleForTesting int LOW_MEMORY_CT_BACKGROUND_KEEP_ALIVE = 8;
        @VisibleForTesting int NUM_ENTRIES = 9;
    }

    /** To be called when a client gets disconnected. */
    public interface DisconnectCallback {
        public void run(CustomTabsSessionToken session);
    }

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
        private EngagementSignalsCallback mEngagementSignalsCallback;
        public final DisconnectCallback disconnectCallback;
        public final PostMessageHandler postMessageHandler;
        public final PostMessageServiceConnection serviceConnection;
        public final Set<Origin> mLinkedOrigins = new HashSet<>();
        public ChromeOriginVerifier originVerifier;
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
        private boolean mCustomTabIsInForeground;
        private boolean mWasSessionDisconnectStatusLogged;
        private Supplier<Boolean> mEngagementSignalsAvailableSupplier;
        private final EngagementSignalsHandler mEngagementSignalsHandler;

        public SessionParams(
                Context context,
                int uid,
                CustomTabsCallback customTabsCallback,
                DisconnectCallback callback,
                PostMessageHandler postMessageHandler,
                PostMessageServiceConnection serviceConnection,
                EngagementSignalsHandler engagementSignalsHandler) {
            this.uid = uid;
            mPackageName = getPackageName(context, uid);
            mCustomTabsCallback = customTabsCallback;
            disconnectCallback = callback;
            this.postMessageHandler = postMessageHandler;
            this.serviceConnection = serviceConnection;
            if (postMessageHandler != null) this.serviceConnection.setPackageName(mPackageName);
            mEngagementSignalsHandler = engagementSignalsHandler;
        }

        /** Overrides package name with given String. TO be used for testing only. */
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

        public EngagementSignalsCallback getEngagementSignalsCallback() {
            return mEngagementSignalsCallback;
        }

        public void setEngagementSignalsCallback(EngagementSignalsCallback callback) {
            mEngagementSignalsCallback = callback;
        }

        public void setEngagementSignalsAvailableSupplier(Supplier<Boolean> supplier) {
            mEngagementSignalsAvailableSupplier = supplier;
        }

        public Supplier<Boolean> getEngagementSignalsAvailableSupplier() {
            return mEngagementSignalsAvailableSupplier;
        }

        public EngagementSignalsHandler getEngagementSignalsHandler() {
            return mEngagementSignalsHandler;
        }
    }

    /** A wrapper around {@link InstalledAppProviderImpl} to aid testing. */
    interface InstalledAppProviderWrapper {
        /**
         * Calls through to {@link InstalledAppProviderImpl#isAppInstalledAndAssociatedWithOrigin}.
         */
        boolean isAppInstalledAndAssociatedWithOrigin(String packageName, Origin origin);
    }

    private static class ProdInstalledAppProviderWrapper implements InstalledAppProviderWrapper {
        @Override
        public boolean isAppInstalledAndAssociatedWithOrigin(String packageName, Origin origin) {
            return InstalledAppProviderImpl.isAppInstalledAndAssociatedWithOrigin(
                    packageName, new GURL(origin.toString()));
        }
    }

    private final ChromeOriginVerifierFactory mOriginVerifierFactory;
    private final InstalledAppProviderWrapper mInstalledAppProviderWrapper;
    private final ChromeBrowserInitializer mChromeBrowserInitializer;

    private final Map<CustomTabsSessionToken, SessionParams> mSessionParams = new HashMap<>();

    private final SparseBooleanArray mUidHasCalledWarmup = new SparseBooleanArray();
    private boolean mWarmupHasBeenCalled;

    public ClientManager() {
        this(
                new ChromeOriginVerifierFactoryImpl(),
                new ProdInstalledAppProviderWrapper(),
                ChromeBrowserInitializer.getInstance());
    }

    public ClientManager(
            ChromeOriginVerifierFactory originVerifierFactory,
            InstalledAppProviderWrapper installedAppProviderWrapper,
            ChromeBrowserInitializer chromeBrowserInitializer) {
        mOriginVerifierFactory = originVerifierFactory;
        mInstalledAppProviderWrapper = installedAppProviderWrapper;
        mChromeBrowserInitializer = chromeBrowserInitializer;
        RequestThrottler.loadInBackground();
    }

    /**
     * Creates a new session.
     *
     * @param session Session provided by the client.
     * @param uid Client UID, as returned by Binder.getCallingUid(),
     * @param onDisconnect To be called on the UI thread when a client gets disconnected.
     * @param postMessageHandler The handler to be used for postMessage related operations.
     * @return true for success.
     */
    public synchronized boolean newSession(
            CustomTabsSessionToken session,
            int uid,
            DisconnectCallback onDisconnect,
            @NonNull PostMessageHandler postMessageHandler,
            @NonNull PostMessageServiceConnection serviceConnection,
            @NonNull EngagementSignalsHandler engagementSignalsHandler) {
        if (session == null || session.getCallback() == null) return false;
        if (mSessionParams.containsKey(session)) {
            SessionParams params = mSessionParams.get(session);
            params.setCustomTabsCallback(session.getCallback());
            params.mWasSessionDisconnectStatusLogged = false;
        } else {
            SessionParams params =
                    new SessionParams(
                            ContextUtils.getApplicationContext(),
                            uid,
                            session.getCallback(),
                            onDisconnect,
                            postMessageHandler,
                            serviceConnection,
                            engagementSignalsHandler);
            mSessionParams.put(session, params);
        }

        return true;
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
        int result =
                mWarmupHasBeenCalled
                        ? CalledWarmup.NO_SESSION_WARMUP
                        : CalledWarmup.NO_SESSION_NO_WARMUP;
        if (hasValidSession) {
            if (hasUidCalledWarmup) {
                result = CalledWarmup.SESSION_WARMUP;
            } else {
                result =
                        mWarmupHasBeenCalled
                                ? CalledWarmup.SESSION_NO_WARMUP_ALREADY_CALLED
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

        boolean urlsMatch =
                TextUtils.equals(predictedUrl, url)
                        || (params.mIgnoreFragments
                                && UrlUtilities.urlsMatchIgnoringFragments(predictedUrl, url));
        return urlsMatch ? PredictionStatus.GOOD : PredictionStatus.BAD;
    }

    /** Registers that a client has launched a URL inside a Custom Tab. */
    public synchronized void registerLaunch(CustomTabsSessionToken session, String url) {
        @PredictionStatus int outcome = getPredictionOutcome(session, url);

        SessionParams params = mSessionParams.get(session);
        if (outcome == PredictionStatus.GOOD) {
            RequestThrottler.getForUid(params.uid).registerSuccess(params.mPredictedUrl);
        }
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.WarmupStateOnLaunch",
                getWarmupState(session),
                CalledWarmup.NUM_ENTRIES);

        if (params == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.MayLaunchUrlType",
                    MayLaunchUrlType.INVALID_SESSION,
                    MayLaunchUrlType.NUM_ENTRIES);
            return;
        }

        @MayLaunchUrlType
        int value =
                (params.lowConfidencePrediction ? MayLaunchUrlType.LOW_CONFIDENCE : 0)
                        + (params.highConfidencePrediction ? MayLaunchUrlType.HIGH_CONFIDENCE : 0);
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.MayLaunchUrlType", value, MayLaunchUrlType.NUM_ENTRIES);
        params.resetPredictionMetrics();
    }

    public int postMessage(CustomTabsSessionToken session, String message) {
        return callOnSession(
                session,
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                params -> params.postMessageHandler.postMessageFromClientApp(message));
    }

    /**
     * See {@link PostMessageServiceConnection#bindSessionToPostMessageService(Context, String)}.
     */
    public boolean bindToPostMessageServiceForSession(CustomTabsSessionToken session) {
        return callOnSession(
                session,
                false,
                params ->
                        params.serviceConnection.bindSessionToPostMessageService(
                                ContextUtils.getApplicationContext()));
    }

    /** See {@link PostMessageHandler#initializeWithPostMessageUri(Uri, Uri)}. */
    public void initializeWithPostMessageOriginForSession(
            CustomTabsSessionToken session, Uri origin, Uri targetOrigin) {
        callOnSession(
                session,
                params ->
                        params.postMessageHandler.initializeWithPostMessageUri(
                                origin, targetOrigin));
    }

    public synchronized boolean validateRelationship(
            CustomTabsSessionToken session, int relation, Origin origin, Bundle extras) {
        return validateRelationshipInternal(session, relation, origin, null, false, null);
    }

    /** Validates the link between the client and the origin. */
    public synchronized void verifyAndInitializeWithPostMessageOriginForSession(
            CustomTabsSessionToken session,
            Origin origin,
            Origin targetOrigin,
            @Relation int relation) {
        validateRelationshipInternal(session, relation, origin, targetOrigin, true, null);
    }

    /**
     * Call validateRelationship to verify and store whether given origin is valid as a source
     * origin of prefetch.
     *
     * @param session client session.
     * @param sourceOrigin origin to be verified.
     * @param callback callback to be called after verification is finished.
     */
    public synchronized void validateSourceOriginOfPrefetch(
            CustomTabsSessionToken session, Origin sourceOrigin, Runnable callback) {
        validateRelationshipInternal(
                session,
                CustomTabsService.RELATION_USE_AS_ORIGIN,
                sourceOrigin,
                null,
                false,
                callback);
    }

    /** Can't be called on UI Thread. */
    private synchronized boolean validateRelationshipInternal(
            CustomTabsSessionToken session,
            int relation,
            Origin origin,
            @Nullable Origin targetOrigin,
            boolean initializePostMessageChannel,
            Runnable internalCallback) {
        SessionParams params = mSessionParams.get(session);
        if (params == null || TextUtils.isEmpty(params.getPackageName())) return false;

        OriginVerificationListener listener =
                (packageName, verifiedOrigin, verified, online) -> {
                    assert origin.equals(verifiedOrigin);

                    CustomTabsCallback callback = getCallbackForSession(session);
                    if (callback != null) {
                        Bundle extras = null;
                        if (verified && online != null) {
                            extras = new Bundle();
                            extras.putBoolean(CustomTabsCallback.ONLINE_EXTRAS_KEY, online);
                        }
                        callback.onRelationshipValidationResult(
                                relation, origin.uri(), verified, extras);
                    }
                    if (initializePostMessageChannel) {
                        if (targetOrigin != null) {
                            params.postMessageHandler.setPostMessageTargetUri(targetOrigin.uri());
                        }
                        params.postMessageHandler.onOriginVerified(
                                packageName, verifiedOrigin, verified, online);
                    }

                    if (internalCallback != null) {
                        internalCallback.run();
                    }
                };

        params.originVerifier =
                mOriginVerifierFactory.create(
                        params.getPackageName(),
                        relation,
                        /* webContents= */ null,
                        /* externalAuthUtils= */ null);

        mChromeBrowserInitializer.runNowOrAfterFullBrowserStarted(
                () -> {
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                params.originVerifier.start(listener, origin);
                            });
                });
        if (relation == CustomTabsService.RELATION_HANDLE_ALL_URLS
                && mInstalledAppProviderWrapper.isAppInstalledAndAssociatedWithOrigin(
                        params.getPackageName(), origin)) {
            params.mLinkedOrigins.add(origin);
        }
        return true;
    }

    /**
     * @return The postMessage origin for the given session.
     */
    Uri getPostMessageOriginForSessionForTesting(CustomTabsSessionToken session) {
        return callOnSession(
                session,
                null,
                params -> params.postMessageHandler.getPostMessageUriForTesting() // IN-TEST
                );
    }

    /**
     * @return The postMessage target origin for the given session.
     */
    Uri getPostMessageTargetOriginForSessionForTesting(CustomTabsSessionToken session) {
        return callOnSession(
                session,
                null,
                params -> params.postMessageHandler.getPostMessageTargetUriForTesting() // IN-TEST
                );
    }

    /** See {@link PostMessageHandler#reset(WebContents)}. */
    public void resetPostMessageHandlerForSession(
            CustomTabsSessionToken session, WebContents webContents) {
        callOnSession(session, params -> params.postMessageHandler.reset(webContents));
    }

    /**
     * @return The referrer that is associated with the client owning given session.
     */
    public synchronized Referrer getDefaultReferrerForSession(CustomTabsSessionToken session) {
        return IntentHandler.constructValidReferrerForAuthority(
                getClientPackageNameForSession(session));
    }

    /**
     * @return The package name associated with the client owning the given session.
     */
    public String getClientPackageNameForSession(CustomTabsSessionToken session) {
        return callOnSession(session, null, params -> params.getPackageName());
    }

    /**
     * Overrides the package name for the given session to be the given package name. To be used
     * for testing only.
     */
    public void overridePackageNameForSessionForTesting(
            CustomTabsSessionToken session, String packageName) {
        callOnSession(
                session, params -> params.overridePackageNameForTesting(packageName) // IN-TEST
                );
    }

    /**
     * @return The callback {@link CustomTabsSessionToken} for the given session.
     */
    public CustomTabsCallback getCallbackForSession(CustomTabsSessionToken session) {
        return callOnSession(session, null, params -> params.getCustomTabsCallback());
    }

    /**
     * @return Whether the urlbar should be hidden for the session on first page load. Urls are
     *         foced to show up after the user navigates away.
     */
    public boolean shouldHideDomainForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mShouldHideDomain);
    }

    /** Sets whether the urlbar should be hidden for a given session. */
    public void setHideDomainForSession(CustomTabsSessionToken session, boolean hide) {
        callOnSession(session, params -> params.mShouldHideDomain = hide);
    }

    /**
     * @return Whether bottom bar scrolling state should be recorded and shared for the session.
     */
    public boolean shouldSendBottomBarScrollStateForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mShouldSendBottomBarScrollState);
    }

    /** Sets whether bottom bar scrolling state should be recorded and shared for the session. */
    public void setSendBottomBarScrollingStateForSessionn(
            CustomTabsSessionToken session, boolean send) {
        callOnSession(session, params -> params.mShouldSendBottomBarScrollState = send);
    }

    /**
     * @return Whether navigation info should be recorded and shared for the session.
     */
    public boolean shouldSendNavigationInfoForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mShouldSendNavigationInfo);
    }

    /**
     * Sets whether navigation info should be recorded and shared for the current navigation in this
     * session.
     */
    public void setSendNavigationInfoForSession(CustomTabsSessionToken session, boolean send) {
        callOnSession(session, params -> params.mShouldSendNavigationInfo = send);
    }

    /**
     * @return Whether the fragment should be ignored for speculation matching.
     */
    public boolean getIgnoreFragmentsForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mIgnoreFragments);
    }

    /** Sets whether the fragment should be ignored for speculation matching. */
    public void setIgnoreFragmentsForSession(CustomTabsSessionToken session, boolean value) {
        callOnSession(session, params -> params.mIgnoreFragments = value);
    }

    /**
     * @return Whether load speculation should be turned on for cellular networks for given session.
     */
    public boolean shouldSpeculateLoadOnCellularForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mShouldSpeculateLoadOnCellular);
    }

    /**
     * @return Whether the session is using the default parameters (that is, don't ignore
     *         fragments and don't speculate loads on cellular connections).
     */
    public boolean usesDefaultSessionParameters(CustomTabsSessionToken session) {
        return callOnSession(session, true, params -> params.isDefault());
    }

    /**
     * Sets whether speculation should be turned on for mobile networks for given session.
     * If it is turned on, hidden tab speculation is turned on as well.
     */
    public void setSpeculateLoadOnCellularForSession(
            CustomTabsSessionToken session, boolean shouldSpeculate) {
        callOnSession(
                session,
                params -> {
                    params.mShouldSpeculateLoadOnCellular = shouldSpeculate;
                    params.mCanUseHiddenTab = shouldSpeculate;
                });
    }

    /** Sets whether hidden tab speculation can be used. */
    public void setCanUseHiddenTab(CustomTabsSessionToken session, boolean canUseHiddenTab) {
        callOnSession(session, params -> params.mCanUseHiddenTab = canUseHiddenTab);
    }

    /** Get whether hidden tab speculation can be used. The default is false. */
    public boolean getCanUseHiddenTab(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mCanUseHiddenTab);
    }

    public void setAllowParallelRequestForSession(CustomTabsSessionToken session, boolean allowed) {
        callOnSession(session, params -> params.mAllowParallelRequest = allowed);
    }

    public boolean getAllowParallelRequestForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mAllowParallelRequest);
    }

    public void setAllowResourcePrefetchForSession(
            CustomTabsSessionToken session, boolean allowed) {
        callOnSession(session, params -> params.mAllowResourcePrefetch = allowed);
    }

    public boolean getAllowResourcePrefetchForSession(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mAllowResourcePrefetch);
    }

    public void setShouldGetPageLoadMetricsForSession(
            CustomTabsSessionToken session, boolean allowed) {
        callOnSession(session, params -> params.mShouldGetPageLoadMetrics = allowed);
    }

    public boolean shouldGetPageLoadMetrics(CustomTabsSessionToken session) {
        return callOnSession(session, false, params -> params.mShouldGetPageLoadMetrics);
    }

    /** Returns the uid associated with the session, {@code -1} if there is no matching session. */
    public int getUidForSession(CustomTabsSessionToken session) {
        return callOnSession(session, -1, params -> params.uid);
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
        return ChromeOriginVerifier.wasPreviouslyVerified(
                getClientPackageNameForSession(session),
                origin,
                CustomTabsService.RELATION_USE_AS_ORIGIN);
    }

    /** Tries to bind to a client to keep it alive, and returns true for success. */
    public synchronized boolean keepAliveForSession(CustomTabsSessionToken session, Intent intent) {
        // When an application is bound to a service, its priority is raised to
        // be at least equal to the application's one. This binds to a placeholder
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
            connection =
                    new KeepAliveServiceConnection(
                            ContextUtils.getApplicationContext(), serviceIntent);
        }

        boolean ok = connection.connect();
        if (ok) params.setKeepAliveConnection(connection);
        return ok;
    }

    /** Unbind from the KeepAlive service for a client. */
    public void dontKeepAliveForSession(CustomTabsSessionToken session) {
        callOnSession(
                session,
                params -> {
                    KeepAliveServiceConnection connection = params.getKeepAliveConnection();
                    if (connection == null) return;

                    connection.disconnect();
                });
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

    /** Cleans up all data associated with all sessions. */
    public synchronized void cleanupAll() {
        // cleanupSessionInternal modifies mSessionParams therefore we need a copy
        List<CustomTabsSessionToken> sessions = new ArrayList<>(mSessionParams.keySet());
        for (CustomTabsSessionToken session : sessions) cleanupSession(session);
    }

    /**
     * Handle any clean up left after a session is destroyed.
     * @param session The session that has been destroyed.
     */
    private void cleanupSessionInternal(CustomTabsSessionToken session) {
        callOnSession(
                session,
                params -> {
                    logConnectionClosed(params);
                    mSessionParams.remove(session);
                    if (params.serviceConnection != null) {
                        params.serviceConnection.cleanup(ContextUtils.getApplicationContext());
                    }
                    if (params.originVerifier != null) params.originVerifier.cleanUp();
                    if (params.disconnectCallback != null) params.disconnectCallback.run(session);
                    mUidHasCalledWarmup.delete(params.uid);
                });
    }

    /**
     * Destroys session when its callback become invalid if the callback is used as identifier.
     *
     * @param session The session with invalid callback.
     */
    public synchronized void cleanupSession(CustomTabsSessionToken session) {
        if (session.hasId() && mSessionParams.containsKey(session)) {
            SessionParams params = mSessionParams.get(session);
            // Logging as soon as we know a session has been disconnected.
            logConnectionClosed(params);
            // Leave session parameters, so client might update callback later.
            // The session will be completely removed when system runs low on memory.
            // {@see #cleanupUnusedSessions}
            params.setCustomTabsCallback(null);
        } else {
            cleanupSessionInternal(session);
        }
    }

    /** Clean up all sessions which are not currently used. */
    public synchronized void cleanupUnusedSessions() {
        // cleanupSessionInternal modifies mSessionParams therefore we need a copy
        List<CustomTabsSessionToken> sessions = new ArrayList<>(mSessionParams.keySet());
        for (CustomTabsSessionToken session : sessions) {
            if (mSessionParams.get(session).getCustomTabsCallback() == null) {
                cleanupSessionInternal(session);
            }
        }
    }

    public void setCustomTabIsInForeground(
            @Nullable CustomTabsSessionToken session, boolean isInForeground) {
        callOnSession(
                session,
                params -> {
                    params.mCustomTabIsInForeground = isInForeground;
                });
    }

    public void setEngagementSignalsCallbackForSession(
            CustomTabsSessionToken session, EngagementSignalsCallback callback) {
        callOnSession(session, params -> params.setEngagementSignalsCallback(callback));
    }

    public @Nullable EngagementSignalsCallback getEngagementSignalsCallbackForSession(
            CustomTabsSessionToken session) {
        return callOnSession(session, null, SessionParams::getEngagementSignalsCallback);
    }

    public void setEngagementSignalsAvailableSupplierForSession(
            CustomTabsSessionToken session, Supplier<Boolean> supplier) {
        callOnSession(session, params -> params.setEngagementSignalsAvailableSupplier(supplier));
    }

    public @Nullable Supplier<Boolean> getEngagementSignalsAvailableSupplierForSession(
            CustomTabsSessionToken session) {
        return callOnSession(session, null, SessionParams::getEngagementSignalsAvailableSupplier);
    }

    public @Nullable EngagementSignalsHandler getEngagementSignalsHandlerForSession(
            CustomTabsSessionToken session) {
        return callOnSession(session, null, SessionParams::getEngagementSignalsHandler);
    }

    private void logConnectionClosed(SessionParams sessionParams) {
        if (sessionParams.mWasSessionDisconnectStatusLogged) return;

        boolean isCustomTabInForeground = sessionParams.mCustomTabIsInForeground;
        boolean isKeepAlive = sessionParams.getKeepAliveConnection() != null;
        boolean isLowMemory = SysUtils.isCurrentlyLowMemory();

        @SessionDisconnectStatus int status = SessionDisconnectStatus.UNKNOWN;
        if (isLowMemory && isCustomTabInForeground && isKeepAlive) {
            status = SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND_KEEP_ALIVE;
        } else if (isLowMemory && isCustomTabInForeground && !isKeepAlive) {
            status = SessionDisconnectStatus.LOW_MEMORY_CT_FOREGROUND;
        } else if (isLowMemory && !isCustomTabInForeground && isKeepAlive) {
            status = SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND_KEEP_ALIVE;
        } else if (isLowMemory && !isCustomTabInForeground && !isKeepAlive) {
            status = SessionDisconnectStatus.LOW_MEMORY_CT_BACKGROUND;
        } else if (isCustomTabInForeground && !isKeepAlive) {
            status = SessionDisconnectStatus.CT_FOREGROUND;
        } else if (isCustomTabInForeground && isKeepAlive) {
            status = SessionDisconnectStatus.CT_FOREGROUND_KEEP_ALIVE;
        } else if (!isCustomTabInForeground && !isKeepAlive) {
            status = SessionDisconnectStatus.CT_BACKGROUND;
        } else if (!isCustomTabInForeground && isKeepAlive) {
            status = SessionDisconnectStatus.CT_BACKGROUND_KEEP_ALIVE;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.SessionDisconnectStatus", status, SessionDisconnectStatus.NUM_ENTRIES);

        sessionParams.mWasSessionDisconnectStatusLogged = true;
    }

    private interface SessionParamsCallback<T> {
        T run(SessionParams params);
    }

    private synchronized <T> T callOnSession(
            CustomTabsSessionToken session, T fallback, SessionParamsCallback<T> callback) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return fallback;
        return callback.run(params);
    }

    private interface SessionParamsRunnable {
        void run(SessionParams params);
    }

    private synchronized void callOnSession(
            CustomTabsSessionToken session, SessionParamsRunnable runnable) {
        SessionParams params = mSessionParams.get(session);
        if (params == null) return;
        runnable.run(params);
    }
}
