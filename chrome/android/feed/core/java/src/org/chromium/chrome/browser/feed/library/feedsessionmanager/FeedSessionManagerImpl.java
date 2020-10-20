// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.base.Function;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent.Listener;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.SessionState;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.ActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.intern.HashPoolInterner;
import org.chromium.chrome.browser.feed.library.common.intern.InternedMap;
import org.chromium.chrome.browser.feed.library.common.intern.Interner;
import org.chromium.chrome.browser.feed.library.common.intern.InternerWithStats;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.logging.StringFormattingUtils;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.ContentCache;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.HeadAsStructure;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.HeadAsStructure.TreeNode;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.InitializableSession;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.Session;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionCache;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionFactory;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionManagerMutation;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.PietSharedStateItemProto.PietSharedStateItem;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.annotation.concurrent.GuardedBy;

/** Implementation of the FeedSessionManager. All state is kept in memory. */
public final class FeedSessionManagerImpl
        implements FeedSessionManager, StoreListener, FeedLifecycleListener, Dumpable {
    private static final String TAG = "FeedSessionManagerImpl";

    private static final long TIMEOUT_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(5L);

    // For the Shared State we will always cache them in the Session Manager
    // Accessed on main thread, updated on Executor task
    private final InternerWithStats<StreamSharedState> mSharedStateInterner =
            new InternerWithStats<>(new StreamSharedStateInterner());
    private final Map<String, StreamSharedState> mSharedStateCache =
            new InternedMap<>(new HashMap<>(), mSharedStateInterner);

    // All writes are done on background threads, there are accesses on the main thread.  Leaving
    // the lock back accessibleContentSupplier may eventually run on a background task and not
    // on the executor thread.
    private final SessionCache mSessionCache;

    // All access to the content cache happens on the executor thread so there is no need to
    // synchronize access.
    private final ContentCache mContentCache;

    @VisibleForTesting
    final AtomicBoolean mInitialized = new AtomicBoolean(false);

    private final Object mLock = new Object();

    // Keep track of sessions being created which haven't been added to the SessionCache.
    // This is accessed on the main thread and the background thread.
    @GuardedBy("mLock")
    private final List<InitializableSession> mSessionsUnderConstruction = new ArrayList<>();

    // This captures the NO_CARDS_ERROR when a request fails. The request fails in one task and this
    // is sent to the ModelProvider in the populateSessionTask.
    @Nullable
    private ModelError mNoCardsError;

    private final SessionFactory mSessionFactory;
    private final SessionManagerMutation mSessionManagerMutation;
    private final Store mStore;
    private final ThreadUtils mThreadUtils;
    private final TimingUtils mTimingUtils;
    private final ProtocolAdapter mProtocolAdapter;
    private final FeedRequestManager mRequestManager;
    private final ActionUploadRequestManager mActionUploadRequestManager;
    private final SchedulerApi mSchedulerApi;
    private final TaskQueue mTaskQueue;
    private final Clock mClock;
    private final Configuration mConfiguration;
    private final MainThreadRunner mMainThreadRunner;
    private final BasicLoggingApi mBasicLoggingApi;
    private final ActionManager mActionManager;
    private final long mSessionPopulationTimeoutMs;
    private final boolean mUploadingActionsEnabled;

    @VisibleForTesting
    final Set<SessionMutationTracker> mOutstandingMutations = new HashSet<>();

    // operation counts for the dumper
    private int mNewSessionCount;
    private int mExistingSessionCount;
    private int mHandleTokenCount;
    private Listener mKnownContentListener;

    @SuppressWarnings("argument.type.incompatible") // ok call to registerObserver
    public FeedSessionManagerImpl(TaskQueue taskQueue, SessionFactory sessionFactory,
            SessionCache sessionCache, SessionManagerMutation sessionManagerMutation,
            ContentCache contentCache, Store store, TimingUtils timingUtils,
            ThreadUtils threadUtils, ProtocolAdapter protocolAdapter,
            FeedRequestManager feedRequestManager,
            ActionUploadRequestManager actionUploadRequestManager, SchedulerApi schedulerApi,
            Configuration configuration, Clock clock,
            FeedObservable<FeedLifecycleListener> lifecycleListenerObservable,
            MainThreadRunner mainThreadRunner, BasicLoggingApi basicLoggingApi,
            ActionManager actionManager) {
        this.mTaskQueue = taskQueue;
        this.mSessionFactory = sessionFactory;
        this.mSessionCache = sessionCache;
        this.mSessionManagerMutation = sessionManagerMutation;
        this.mContentCache = contentCache;

        this.mStore = store;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mProtocolAdapter = protocolAdapter;
        this.mRequestManager = feedRequestManager;
        this.mActionUploadRequestManager = actionUploadRequestManager;
        this.mSchedulerApi = schedulerApi;
        this.mClock = clock;
        this.mConfiguration = configuration;
        this.mMainThreadRunner = mainThreadRunner;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mActionManager = actionManager;
        mUploadingActionsEnabled =
                configuration.getValueOrDefault(ConfigKey.UNDOABLE_ACTIONS_ENABLED, false);
        mSessionPopulationTimeoutMs =
                configuration.getValueOrDefault(ConfigKey.TIMEOUT_TIMEOUT_MS, TIMEOUT_TIMEOUT_MS);
        lifecycleListenerObservable.registerObserver(this);
        Logger.i(TAG, "FeedSessionManagerImpl has been created");
    }

    /**
     * Called to initialize the session manager. This creates an executor task which does the actual
     * work of setting up the current state. If this is not called, the session manager will not
     * populate new or existing sessions. There isn't error checking on this since this happens on
     * an executor task.
     */
    public void initialize() {
        boolean init = mInitialized.getAndSet(true);
        if (init) {
            Logger.w(TAG, "FeedSessionManagerImpl has previously been initialized");
            return;
        }
        mStore.registerObserver(this);
        mTaskQueue.initialize(this::initializationTask);
    }

    // Task which initializes the Session Manager.  This must be the first task run on the
    // Session Manager thread so it's complete before we create any sessions.
    private void initializationTask() {
        mThreadUtils.checkNotMainThread();
        Thread currentThread = Thread.currentThread();
        currentThread.setName("JardinExecutor");
        mTimingUtils.pinThread(currentThread, "JardinExecutor");

        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        // Initialize the Shared States cached here.
        ElapsedTimeTracker sharedStateTimeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Result<List<StreamSharedState>> sharedStatesResult = mStore.getSharedStates();
        if (sharedStatesResult.isSuccessful()) {
            for (StreamSharedState sharedState : sharedStatesResult.getValue()) {
                mSharedStateCache.put(sharedState.getContentId(), sharedState);
            }
        } else {
            // without shared states we need to switch to ephemeral mode
            switchToEphemeralMode("SharedStates failed to load, no shared states are loaded.");
            mTaskQueue.reset();
            sharedStateTimeTracker.stop("", "sharedStateTimeTracker", "error", "store error");
            timeTracker.stop("task", "Initialization", "error", "switchToEphemeralMode");
            return;
        }
        sharedStateTimeTracker.stop("", "sharedStateTimeTracker");

        // create the head session from the data in the Store
        if (!mSessionCache.initialize()) {
            // we failed to initialize the sessionCache, so switch to ephemeral mode.
            switchToEphemeralMode("unable to initialize the sessionCache");
            timeTracker.stop("task", "Initialization", "error", "switchToEphemeralMode");
            return;
        }
        timeTracker.stop("task", "Initialization");
    }

    @Override
    public void getNewSession(ModelProvider modelProvider,
            @Nullable ViewDepthProvider viewDepthProvider, UiContext uiContext) {
        mThreadUtils.checkMainThread();
        if (!mInitialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getNewSession");
            initialize();
        }
        InitializableSession session = mSessionFactory.getSession();
        session.bindModelProvider(modelProvider, viewDepthProvider);
        synchronized (mLock) {
            mSessionsUnderConstruction.add(session);
        }

        if (!mSessionCache.isHeadInitialized()) {
            Logger.i(TAG, "Delaying populateSession until initialization is finished");
            mTaskQueue.execute(Task.GET_NEW_SESSION, TaskType.IMMEDIATE,
                    () -> populateSession(session, uiContext));
        } else {
            populateSession(session, uiContext);
        }
    }

    // This method can be run either on the main thread or on the background thread. It calls the
    // SchedulerApi to determine how the session is created. It creates a new task to populate the
    // new session.
    private void populateSession(InitializableSession session, UiContext uiContext) {
        // Create SessionState and call SchedulerApi to determine what the session-creation
        // experience should be.
        SessionState sessionState = new SessionState(!mSessionCache.getHead().isHeadEmpty(),
                mSessionCache.getHeadLastAddedTimeMillis(), mTaskQueue.isMakingRequest());
        Logger.i(TAG,
                "shouldSessionRequestData; hasContent(%b), contentCreationTime(%d),"
                        + " outstandingRequest(%b)",
                sessionState.hasContent, sessionState.contentCreationDateTimeMs,
                sessionState.hasOutstandingRequest);
        @RequestBehavior
        int behavior = mSchedulerApi.shouldSessionRequestData(sessionState);

        // Based on sessionState and behavior, determine if FeedSessionManager should start a
        // request, append an ongoing request to this session, or include a timeout.
        boolean shouldAppendOutstandingRequest = shouldAppendToSession(sessionState, behavior);
        boolean shouldStartRequest = shouldStartRequest(sessionState, behavior);
        Runnable timeoutTask = shouldPopulateWithTimeout(sessionState, behavior) ? ()
                -> populateSessionTask(session, shouldAppendOutstandingRequest, uiContext)
                : null;
        Logger.i(TAG,
                "shouldSessionRequestDataResult: %s, shouldMakeRequest(%b), withTimeout(%b),"
                        + " withAppend(%b)",
                requestBehaviorToString(behavior), shouldStartRequest, timeoutTask != null,
                shouldAppendOutstandingRequest);

        // If we are making a request, there are two orders, request -> populate for all cases
        // except for REQUEST_WITH_CONTENT which is populate -> request.
        if (shouldStartRequest && behavior != RequestBehavior.REQUEST_WITH_CONTENT) {
            triggerRefresh(/* sessionId= */ null, RequestReason.OPEN_WITHOUT_CONTENT, uiContext);
        }
        mTaskQueue.execute(Task.POPULATE_NEW_SESSION, requestBehaviorToTaskType(behavior),
                ()
                        -> populateSessionTask(session, shouldAppendOutstandingRequest, uiContext),
                timeoutTask, mSessionPopulationTimeoutMs);
        if (shouldStartRequest && behavior == RequestBehavior.REQUEST_WITH_CONTENT) {
            triggerRefresh(/* sessionId= */ null, RequestReason.OPEN_WITH_CONTENT, uiContext);
        }
    }

    private void populateSessionTask(InitializableSession session,
            boolean shouldAppendOutstandingRequest, UiContext uiContext) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);

        if (mNoCardsError != null && mSessionCache.getHead().isHeadEmpty()) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider == null) {
                Logger.e(TAG, "populateSessionTask - noCardsError, modelProvider not found");
                timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
                return;
            }
            Logger.w(TAG, "populateSessionTask - noCardsError %s", modelProvider);

            Result<String> streamSessionResult = mStore.createNewSession();
            if (!streamSessionResult.isSuccessful()) {
                switchToEphemeralMode("Unable to create a new session during noCardsError failure");
                timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
                return;
            }

            // properly track the session so that it's empty.
            modelProvider.raiseError(Validators.checkNotNull(mNoCardsError));
            String sessionId = streamSessionResult.getValue();
            session.setSessionId(sessionId);
            mSessionCache.putAttached(sessionId, mClock.currentTimeMillis(),
                    mSessionCache.getHead().getSchemaVersion(), session);
            synchronized (mLock) {
                mSessionsUnderConstruction.remove(session);
            }

            // Set the session id on the ModelProvider.
            modelProvider.edit().setSessionId(sessionId).commit();
            timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
            return;
        }

        Result<String> streamSessionResult = mStore.createNewSession();
        if (!streamSessionResult.isSuccessful()) {
            switchToEphemeralMode("Unable to create a new session, createNewSession failed");
            timeTracker.stop("task", "Create/Populate New Session", "Failure", "createNewSession");
            return;
        }
        String sessionId = streamSessionResult.getValue();
        session.setSessionId(sessionId);
        Result<List<StreamStructure>> streamStructuresResult =
                mStore.getStreamStructures(sessionId);
        if (!streamStructuresResult.isSuccessful()) {
            switchToEphemeralMode("Unable to create a new session, getStreamStructures failed");
            timeTracker.stop(
                    "task", "Create/Populate New Session", "Failure", "getStreamStructures");
            return;
        }

        boolean cachedBindings;
        cachedBindings = mContentCache.size() > 0;
        long creationTimeMillis = mClock.currentTimeMillis();
        session.populateModelProvider(streamStructuresResult.getValue(), cachedBindings,
                shouldAppendOutstandingRequest, uiContext);
        mSessionCache.putAttached(
                sessionId, creationTimeMillis, mSessionCache.getHead().getSchemaVersion(), session);
        synchronized (mLock) {
            mSessionsUnderConstruction.remove(session);
        }
        mNewSessionCount++;
        Logger.i(TAG, "Populate new session: %s, creation time %s", session.getSessionId(),
                StringFormattingUtils.formatLogDate(creationTimeMillis));
        timeTracker.stop("task", "Create/Populate New Session");
    }

    @VisibleForTesting
    void switchToEphemeralMode(String message) {
        Logger.e(TAG, message);
        mStore.switchToEphemeralMode();
    }

    @VisibleForTesting
    void modelErrorObserver(@Nullable Session session, ModelError error) {
        if (session == null && error.getErrorType() == ErrorType.NO_CARDS_ERROR) {
            Logger.e(TAG, "No Cards Found on TriggerRefresh, setting noCardsError");
            mNoCardsError = error;
            // queue a clear which will run after all currently delayed tasks.  This allows delayed
            // session population tasks to inform the ModelProvider of errors then we clear the
            // error state.
            mTaskQueue.execute(
                    Task.NO_CARD_ERROR_CLEAR, TaskType.USER_FACING, () -> mNoCardsError = null);
            return;
        } else if (session != null && error.getErrorType() == ErrorType.PAGINATION_ERROR) {
            Logger.e(TAG, "Pagination Error found");
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.raiseError(error);
            } else {
                Logger.e(TAG, "handling Pagination Error, didn't find Model Provider");
            }
            return;
        }
        Logger.e(TAG, "unhandled modelErrorObserver: session, %s, error %s", session != null,
                error.getErrorType());
    }

    @Override
    public void getExistingSession(
            String sessionId, ModelProvider modelProvider, UiContext uiContext) {
        mThreadUtils.checkMainThread();
        if (!mInitialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getExistingSession");
            initialize();
        }
        InitializableSession session = mSessionFactory.getSession();
        session.bindModelProvider(modelProvider, null);

        // Task which populates the newly created session.  This must be done
        // on the Session Manager thread so it atomic with the mutations.
        mTaskQueue.execute(Task.GET_EXISTING_SESSION, TaskType.IMMEDIATE, () -> {
            mThreadUtils.checkNotMainThread();
            if (!mSessionCache.hasSession(sessionId)) {
                modelProvider.invalidate(uiContext);
                return;
            }
            Session existingSession = mSessionCache.getAttached(sessionId);
            if (existingSession != null && !existingSession.getContentInSession().isEmpty()) {
                ModelProvider existingModelProvider = existingSession.getModelProvider();
                if (existingModelProvider != null) {
                    existingModelProvider.invalidate(uiContext);
                }
            }

            Result<List<StreamStructure>> streamStructuresResult =
                    mStore.getStreamStructures(sessionId);
            if (streamStructuresResult.isSuccessful()) {
                session.setSessionId(sessionId);
                session.populateModelProvider(
                        streamStructuresResult.getValue(), false, false, uiContext);
                mSessionCache.putAttachedAndRetainMetadata(sessionId, session);
                mExistingSessionCount++;
            } else {
                Logger.e(TAG, "unable to get stream structure for existing session %s", sessionId);
                switchToEphemeralMode("unable to get stream structure for existing session");
            }
        });
    }

    @Override
    public void invalidateSession(String sessionId) {
        if (mThreadUtils.isMainThread()) {
            mTaskQueue.execute(Task.INVALIDATE_SESSION, TaskType.USER_FACING,
                    () -> mSessionCache.removeAttached(sessionId));
        } else {
            mSessionCache.removeAttached(sessionId);
        }
    }

    @Override
    public void detachSession(String sessionId) {
        if (mThreadUtils.isMainThread()) {
            mTaskQueue.execute(Task.DETACH_SESSION, TaskType.USER_FACING,
                    () -> mSessionCache.detachModelProvider(sessionId));
        } else {
            mSessionCache.detachModelProvider(sessionId);
        }
    }

    @Override
    public void invalidateHead() {
        mSessionManagerMutation.resetHead();
    }

    @Override
    public void handleToken(String sessionId, StreamToken streamToken) {
        Logger.i(TAG, "HandleToken on stream %s, token %s", sessionId, streamToken.getContentId());
        mThreadUtils.checkMainThread();

        // At the moment, this doesn't try to prevent multiple requests with the same Token.
        // We may want to make sure we only make the request a single time.
        mHandleTokenCount++;
        MutationContext mutationContext = new MutationContext.Builder()
                                                  .setContinuationToken(streamToken)
                                                  .setRequestingSessionId(sessionId)
                                                  .build();
        mTaskQueue.execute(Task.HANDLE_TOKEN, TaskType.BACKGROUND, () -> {
            fetchActionsAndUpload(getConsistencyToken(), result -> {
                ConsistencyToken token = handleUpdateConsistencyToken(result);
                mRequestManager.loadMore(
                        streamToken, token, getCommitter("handleToken", mutationContext));
            });
        });
    }

    @Override
    public void triggerRefresh(@Nullable String sessionId) {
        triggerRefresh(sessionId, RequestReason.HOST_REQUESTED, UiContext.getDefaultInstance());
    }

    @Override
    public void triggerRefresh(
            @Nullable String sessionId, @RequestReason int requestReason, UiContext uiContext) {
        if (!mInitialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, triggerRefresh");
            initialize();
        }
        mTaskQueue.execute(Task.SESSION_MANAGER_TRIGGER_REFRESH,
                TaskType.HEAD_INVALIDATE, // invalidate because we are requesting a refresh
                () -> triggerRefreshTask(sessionId, requestReason, uiContext));
    }

    private ConsistencyToken handleUpdateConsistencyToken(Result<ConsistencyToken> result) {
        mThreadUtils.checkNotMainThread();
        if (!mUploadingActionsEnabled) {
            return getConsistencyToken();
        }

        ConsistencyToken consistencyToken;
        if (result.isSuccessful()) {
            consistencyToken = result.getValue();
            mStore.editContent()
                    .add(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID,
                            StreamPayload.newBuilder()
                                    .setConsistencyToken(consistencyToken)
                                    .build())
                    .commit();

        } else {
            consistencyToken = getConsistencyToken();
            Logger.w(TAG, "TriggerRefresh didn't get a consistencyToken Back");
        }
        return consistencyToken;
    }

    private void triggerRefreshTask(
            @Nullable String sessionId, @RequestReason int requestReason, UiContext uiContext) {
        mThreadUtils.checkNotMainThread();

        fetchActionsAndUpload(getConsistencyToken(), result -> {
            ConsistencyToken consistencyToken = handleUpdateConsistencyToken(result);
            mRequestManager.triggerRefresh(requestReason, consistencyToken,
                    getCommitter("triggerRefresh",
                            new MutationContext.Builder().setUiContext(uiContext).build()));
        });

        if (sessionId != null) {
            Session session = mSessionCache.getAttached(sessionId);
            if (session != null) {
                ModelProvider modelProvider = session.getModelProvider();
                if (modelProvider != null) {
                    invalidateSessionInternal(modelProvider, session, uiContext);
                } else {
                    Logger.w(TAG, "Session didn't have a ModelProvider %s", sessionId);
                }
            } else {
                Logger.w(TAG, "TriggerRefresh didn't find session %s", sessionId);
            }
        } else {
            Logger.i(TAG, "triggerRefreshTask no StreamSession provided");
        }
    }

    @Override
    public void fetchActionsAndUpload(Consumer<Result<ConsistencyToken>> consumer) {
        mThreadUtils.checkNotMainThread();
        fetchActionsAndUpload(getConsistencyToken(), consumer);
    }

    private void fetchActionsAndUpload(
            ConsistencyToken token, Consumer<Result<ConsistencyToken>> consumer) {
        // fail fast if we're not actually recording these actions.
        if (!mUploadingActionsEnabled) {
            consumer.accept(Result.success(token));
            return;
        }
        mActionUploadRequestManager.triggerUploadAllActions(token, consumer);
    }

    @Override
    public Result<List<PayloadWithId>> getStreamFeatures(List<String> contentIds) {
        mThreadUtils.checkNotMainThread();
        List<PayloadWithId> results = new ArrayList<>();
        List<String> cacheMisses = new ArrayList<>();
        int contentSize = mContentCache.size();
        for (String contentId : contentIds) {
            StreamPayload payload = mContentCache.get(contentId);
            if (payload != null) {
                results.add(new PayloadWithId(contentId, payload));
            } else {
                cacheMisses.add(contentId);
            }
        }

        if (!cacheMisses.isEmpty()) {
            Result<List<PayloadWithId>> contentResult = mStore.getPayloads(cacheMisses);
            boolean successfulRead = contentResult.isSuccessful()
                    && (contentResult.getValue().size()
                                    + mConfiguration.getValueOrDefault(
                                            ConfigKey.STORAGE_MISS_THRESHOLD, 4L)
                            >= cacheMisses.size());
            if (successfulRead) {
                Logger.i(TAG, "getStreamFeatures; requestedItems(%d), result(%d)",
                        cacheMisses.size(), contentResult.getValue().size());
                if (contentResult.getValue().size() < cacheMisses.size()) {
                    Logger.e(TAG, "ContentStorage is missing content");
                    mMainThreadRunner.execute("CONTENT_STORAGE_MISSING_ITEM", () -> {
                        mBasicLoggingApi.onInternalError(
                                InternalFeedError.CONTENT_STORAGE_MISSING_ITEM);
                    });
                }
                results.addAll(contentResult.getValue());
            } else {
                if (contentResult.isSuccessful()) {
                    Logger.e(TAG, "Storage miss beyond threshold; requestedItems(%d), returned(%d)",
                            cacheMisses.size(), contentResult.getValue().size());
                    mMainThreadRunner.execute("STORAGE_MISS_BEYOND_THRESHOLD", () -> {
                        mBasicLoggingApi.onInternalError(
                                InternalFeedError.STORAGE_MISS_BEYOND_THRESHOLD);
                    });
                }

                // since we couldn't populate the content, switch to ephemeral mode
                switchToEphemeralMode("Unable to get the payloads in getStreamFeatures");
                return Result.failure();
            }
        }
        Logger.i(TAG, "Caching getStreamFeatures - items %s, cache misses %s, cache size %s",
                contentIds.size(), cacheMisses.size(), contentSize);
        return Result.success(results);
    }

    @Override
    @Nullable
    public StreamSharedState getSharedState(ContentId contentId) {
        mThreadUtils.checkMainThread();
        String sharedStateId = mProtocolAdapter.getStreamContentId(contentId);
        StreamSharedState state = mSharedStateCache.get(sharedStateId);
        if (state == null) {
            Logger.e(TAG, "Shared State [%s] was not found", sharedStateId);
        }
        return state;
    }

    @Override
    public Consumer<Result<Model>> getUpdateConsumer(MutationContext mutationContext) {
        if (!mInitialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getUpdateConsumer");
            initialize();
        }
        return new SessionMutationTracker(mutationContext, "updateConsumer");
    }

    @VisibleForTesting
    class SessionMutationTracker implements Consumer<Result<Model>> {
        private final MutationContext mMutationContext;
        private final String mTaskName;

        @SuppressWarnings("argument.type.incompatible") // ok to add this to the map
        private SessionMutationTracker(MutationContext mutationContext, String taskName) {
            this.mMutationContext = mutationContext;
            this.mTaskName = taskName;
            mOutstandingMutations.add(this);
        }

        @Override
        public void accept(Result<Model> input) {
            if (mOutstandingMutations.remove(this)) {
                if (input.isSuccessful()) {
                    updateSharedStateCache(input.getValue().streamDataOperations);
                }
                mSessionManagerMutation
                        .createCommitter(mTaskName, mMutationContext,
                                FeedSessionManagerImpl.this::modelErrorObserver,
                                mKnownContentListener)
                        .accept(input);
            } else {
                Logger.w(TAG, "SessionMutationTracker dropping response due to clear");
            }
        }
    }

    @Override
    public <T> void getStreamFeaturesFromHead(
            Function<StreamPayload, T> filterPredicate, Consumer<Result<List<T>>> consumer) {
        mTaskQueue.execute(Task.GET_STREAM_FEATURES_FROM_HEAD, TaskType.BACKGROUND, () -> {
            HeadAsStructure headAsStructure =
                    new HeadAsStructure(mStore, mTimingUtils, mThreadUtils);
            Function<TreeNode, T> toStreamPayload =
                    treeNode -> filterPredicate.apply(treeNode.getStreamPayload());
            headAsStructure.initialize(result -> {
                if (!result.isSuccessful()) {
                    consumer.accept(Result.failure());
                    return;
                }
                Result<List<T>> filterResults = headAsStructure.filter(toStreamPayload);
                consumer.accept(filterResults.isSuccessful()
                                ? Result.success(filterResults.getValue())
                                : Result.failure());
            });
        });
    }

    @Override
    public void setKnownContentListener(KnownContent.Listener knownContentListener) {
        this.mKnownContentListener = knownContentListener;
    }

    @Override
    public void onSwitchToEphemeralMode() {
        reset();
    }

    private Consumer<Result<Model>> getCommitter(String taskName, MutationContext mutationContext) {
        return new SessionMutationTracker(mutationContext, taskName);
    }

    @Override
    public void reset() {
        mThreadUtils.checkNotMainThread();
        mSessionManagerMutation.forceResetHead();
        mSessionCache.reset();
        // Invalidate all sessions currently under construction
        List<InitializableSession> invalidateSessions;
        synchronized (mLock) {
            invalidateSessions = new ArrayList<>(mSessionsUnderConstruction);
            mSessionsUnderConstruction.clear();
        }
        for (InitializableSession session : invalidateSessions) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.invalidate();
            }
        }
        mContentCache.reset();
        mSharedStateCache.clear();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("newSessionCount").value(mNewSessionCount);
        dumper.forKey("existingSessionCount").value(mExistingSessionCount).compactPrevious();
        dumper.forKey("handleTokenCount").value(mHandleTokenCount).compactPrevious();
        dumper.forKey("sharedStateSize").value(mSharedStateCache.size()).compactPrevious();
        dumper.forKey("sharedStateInternerSize")
                .value(mSharedStateInterner.size())
                .compactPrevious();
        dumper.forKey("sharedStateInternerStats")
                .value(mSharedStateInterner.getStats())
                .compactPrevious();
        dumper.dump(mContentCache);
        dumper.dump(mTaskQueue);
        dumper.dump(mSessionCache);
        dumper.dump(mSessionManagerMutation);
    }

    private void invalidateSessionInternal(
            ModelProvider modelProvider, Session session, UiContext uiContext) {
        mThreadUtils.checkNotMainThread();
        Logger.i(TAG, "Invalidate session %s", session.getSessionId());
        modelProvider.invalidate(uiContext);
    }

    // This is only used in tests to verify the contents of the shared state cache.
    @VisibleForTesting
    Map<String, StreamSharedState> getSharedStateCacheForTest() {
        return new HashMap<>(mSharedStateCache);
    }

    // This is only used in tests to access a the associated sessions.
    @VisibleForTesting
    SessionCache getSessionCacheForTest() {
        return mSessionCache;
    }

    // Called in the integration tests
    @VisibleForTesting
    public boolean isDelayed() {
        return mTaskQueue.isDelayed();
    }

    @Override
    public void onLifecycleEvent(@LifecycleEvent String event) {
        Logger.i(TAG, "onLifecycleEvent %s", event);
        switch (event) {
            case LifecycleEvent.INITIALIZE:
                initialize();
                break;
            case LifecycleEvent.CLEAR_ALL:
                Logger.i(TAG, "CLEAR_ALL will cancel %s mutations", mOutstandingMutations.size());
                mOutstandingMutations.clear();
                break;
            case LifecycleEvent.CLEAR_ALL_WITH_REFRESH:
                Logger.i(TAG, "CLEAR_ALL_WITH_REFRESH will cancel %s mutations",
                        mOutstandingMutations.size());
                mOutstandingMutations.clear();
                break;
            case LifecycleEvent.ENTER_FOREGROUND:
                mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(canUpload());
                break;
            case LifecycleEvent.ENTER_BACKGROUND:
                mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(canUpload());
                break;
            case LifecycleEvent.SIGNED_IN:
                mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(canUpload());
                break;
            case LifecycleEvent.SIGNED_OUT:
                mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(canUpload());
                break;
            default:
                // Do nothing
        }
    }

    private boolean canUpload() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)) {
            return UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .getBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS);
        }
        return true;
    }

    // TODO: implement longer term fix for reading/saving the consistency token
    @Override
    public void triggerUploadActions(Set<StreamUploadableAction> actions) {
        mThreadUtils.checkNotMainThread();

        mActionUploadRequestManager.triggerUploadActions(
                actions, getConsistencyToken(), getConsistencyTokenConsumer());
    }

    @VisibleForTesting
    ConsistencyToken getConsistencyToken() {
        mThreadUtils.checkNotMainThread();

        // don't bother with reading consistencytoken if we're not uploading actions.
        if (!mUploadingActionsEnabled) {
            return ConsistencyToken.getDefaultInstance();
        }
        Result<List<PayloadWithId>> contentResult = mStore.getPayloads(
                Collections.singletonList(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID));
        if (contentResult.isSuccessful()) {
            for (PayloadWithId payload : contentResult.getValue()) {
                if (payload.payload.hasConsistencyToken()) {
                    return payload.payload.getConsistencyToken();
                }
            }
        }
        return ConsistencyToken.getDefaultInstance();
    }

    @VisibleForTesting
    Consumer<Result<ConsistencyToken>> getConsistencyTokenConsumer() {
        return result -> {
            if (result.isSuccessful()) {
                mStore.editContent()
                        .add(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID,
                                StreamPayload.newBuilder()
                                        .setConsistencyToken(result.getValue())
                                        .build())
                        .commit();
            }
        };
    }

    private void updateSharedStateCache(List<StreamDataOperation> updates) {
        for (StreamDataOperation dataOperation : updates) {
            Operation operation = dataOperation.getStreamStructure().getOperation();
            if ((operation == Operation.UPDATE_OR_APPEND)
                    && SessionManagerMutation.validDataOperation(dataOperation)) {
                String contentId = dataOperation.getStreamStructure().getContentId();
                StreamPayload payload = dataOperation.getStreamPayload();
                if (payload.hasStreamSharedState()) {
                    mSharedStateCache.put(contentId, payload.getStreamSharedState());
                }
            }
        }
    }

    private static boolean shouldAppendToSession(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.REQUEST_WITH_TIMEOUT:
                return sessionState.hasContent;
            case RequestBehavior.NO_REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return sessionState.hasContent && sessionState.hasOutstandingRequest;
            default:
                return false;
        }
    }

    private static boolean shouldStartRequest(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        return (requestBehavior == RequestBehavior.REQUEST_WITH_TIMEOUT
                       || requestBehavior == RequestBehavior.REQUEST_WITH_WAIT
                       || requestBehavior == RequestBehavior.REQUEST_WITH_CONTENT)
                && !sessionState.hasOutstandingRequest;
    }

    private static boolean shouldPopulateWithTimeout(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        return requestBehavior == RequestBehavior.REQUEST_WITH_TIMEOUT
                || (requestBehavior == RequestBehavior.NO_REQUEST_WITH_TIMEOUT
                        && sessionState.hasOutstandingRequest);
    }

    private static String requestBehaviorToString(@RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.NO_REQUEST_WITH_CONTENT:
                return "NO_REQUEST_WITH_CONTENT";
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return "NO_REQUEST_WITH_TIMEOUT";
            case RequestBehavior.NO_REQUEST_WITH_WAIT:
                return "NO_REQUEST_WITH_WAIT";
            case RequestBehavior.REQUEST_WITH_CONTENT:
                return "REQUEST_WITH_CONTENT";
            case RequestBehavior.REQUEST_WITH_TIMEOUT:
                return "REQUEST_WITH_TIMEOUT";
            case RequestBehavior.REQUEST_WITH_WAIT:
                return "REQUEST_WITH_WAIT";
            default:
                return "UNKNOWN";
        }
    }

    private static int requestBehaviorToTaskType(@RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.REQUEST_WITH_WAIT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_WAIT:
                // Wait for the request to complete and then show content.
                return TaskType.USER_FACING;
            case RequestBehavior.REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_CONTENT:
                // Show content immediately and append when the request completes.
                return TaskType.IMMEDIATE;
            case RequestBehavior.REQUEST_WITH_TIMEOUT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                // Wait for the request to complete but show current content if the timeout elapses.
                return TaskType.USER_FACING;
            default:
                return TaskType.USER_FACING;
        }
    }

    // Interner that caches the inner PietSharedStateItem from a StreamSharedState, which may
    // sometimes be identical, only the inner content_id differing (see [INTERNAL LINK]).
    @VisibleForTesting
    static class StreamSharedStateInterner implements Interner<StreamSharedState> {
        private final Interner<PietSharedStateItem> mInterner = new HashPoolInterner<>();

        @SuppressWarnings("ReferenceEquality")
        // Intentional reference comparison for interned != orig
        @Override
        public StreamSharedState intern(StreamSharedState input) {
            PietSharedStateItem orig = input.getPietSharedStateItem();
            PietSharedStateItem interned = mInterner.intern(orig);
            if (interned != orig) {
                /*
                 * If interned != orig we have a memoized item and we need to replace the proto with
                 * the modified version.
                 */
                return input.toBuilder().setPietSharedStateItem(interned).build();
            }
            return input;
        }

        @Override
        public void clear() {
            mInterner.clear();
        }

        @Override
        public int size() {
            return mInterner.size();
        }
    }
}
