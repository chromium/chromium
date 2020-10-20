// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.internal.store.Store.HEAD_SESSION_ID;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.SessionState;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.intern.Interner;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedAppLifecycleListener;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.FeedModelProviderFactory;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerImpl.SessionMutationTracker;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerImpl.StreamSharedStateInterner;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.HeadSessionImpl;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.Session;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionCache;
import org.chromium.chrome.browser.feed.library.testing.actionmanager.FakeViewActionManager;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.chrome.browser.feed.library.testing.protocoladapter.FakeProtocolAdapter;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.libraries.testing.UiContextForTestProto.UiContextForTest;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.PietSharedStateItemProto.PietSharedStateItem;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.List;

/** Tests of the {@link FeedSessionManagerImpl} class. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = LocalRobolectricTestRunner.DEFAULT_SDK, manifest = Config.NONE)
public class FeedSessionManagerImplTest {
    private static final MutationContext EMPTY_MUTATION = new MutationContext.Builder().build();
    private static final ContentId SHARED_STATE_ID = ContentId.newBuilder()
                                                             .setContentDomain("piet-shared-state")
                                                             .setId(1)
                                                             .setTable("piet-shared-state")
                                                             .build();
    private static final String SESSION_ID = "session:1";
    private static final int STORAGE_MISS_THRESHOLD = 4;

    private final ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final FakeClock mFakeClock = new FakeClock();
    private final String mRootContentId = mIdGenerators.createRootContentId(0);
    private final TimingUtils mTimingUtils = new TimingUtils();

    private Configuration mConfiguration;
    private FakeActionUploadRequestManager mFakeActionUploadRequestManager;
    private FakeBasicLoggingApi mFakeBasicLoggingApi;
    private FakeMainThreadRunner mFakeMainThreadRunner;
    private FakeProtocolAdapter mFakeProtocolAdapter;
    private FakeFeedRequestManager mFakeRequestManager;
    private FakeStore mFakeStore;
    private FakeTaskQueue mFakeTaskQueue;
    private FakeThreadUtils mFakeThreadUtils;
    private FeedAppLifecycleListener mAppLifecycleListener;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private SchedulerApi mSchedulerApi;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private ActionManager mActionManager;

    @Parameters
    public static List<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean mUploadingActionsEnabled;

    @Before
    public void setUp() {
        initMocks(this);

        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        mConfiguration = new Configuration.Builder()
                                 .put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, mUploadingActionsEnabled)
                                 .put(ConfigKey.STORAGE_MISS_THRESHOLD, STORAGE_MISS_THRESHOLD)
                                 .build();
        mFakeBasicLoggingApi = new FakeBasicLoggingApi();
        mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
        mFakeMainThreadRunner =
                FakeMainThreadRunner.runTasksImmediatelyWithThreadChecks(mFakeThreadUtils);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mAppLifecycleListener = new FeedAppLifecycleListener(mFakeThreadUtils);
        mFakeStore = new FakeStore(mConfiguration, mFakeThreadUtils, mFakeTaskQueue, mFakeClock);
        mFakeActionUploadRequestManager = new FakeActionUploadRequestManager(
                mFakeStore, new FakeViewActionManager(mFakeStore), mFakeThreadUtils);
        mFakeProtocolAdapter = new FakeProtocolAdapter();
        mFakeRequestManager = new FakeFeedRequestManager(
                mFakeThreadUtils, mFakeMainThreadRunner, mFakeProtocolAdapter, mFakeTaskQueue);
        mFakeRequestManager.queueResponse(Response.getDefaultInstance());
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.NO_REQUEST_WITH_CONTENT);
    }

    @Test
    public void testInitialization() {
        StreamSharedState sharedState =
                StreamSharedState.newBuilder()
                        .setContentId(mIdGenerators.createFeatureContentId(0))
                        .setPietSharedStateItem(PietSharedStateItem.getDefaultInstance())
                        .build();
        StreamStructure operation =
                StreamStructure.newBuilder()
                        .setContentId(mIdGenerators.createFeatureContentId(0))
                        .setOperation(StreamStructure.Operation.UPDATE_OR_APPEND)
                        .build();
        mFakeStore.setSharedStates(sharedState).setStreamStructures(HEAD_SESSION_ID, operation);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(mConfiguration);
        assertThat(sessionManager.mInitialized.get()).isFalse();
        sessionManager.initialize();
        assertThat(sessionManager.mInitialized.get()).isTrue();
        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(1);

        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        Session head = sessionCache.getHead();
        assertThat(head).isInstanceOf(HeadSessionImpl.class);
        String itemKey = mIdGenerators.createFeatureContentId(0);
        assertThat(head.getContentInSession()).containsExactly(itemKey);
    }

    // This is testing a condition similar to the one that caused [INTERNAL LINK].
    @Test
    public void testInitialization_equalSharedStatesDifferentContentIds() throws Exception {
        StreamSharedState sharedState1 =
                StreamSharedState.newBuilder()
                        .setContentId("shared-state-1")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addStylesheets(
                                        Stylesheet.newBuilder().setStylesheetId(
                                                "shared-stylesheet"))))
                        .build();
        StreamSharedState sharedState2 =
                StreamSharedState.newBuilder()
                        .setContentId("shared-state-2") //  Different ContentId
                        .setPietSharedStateItem( // Equal PietSharedStateItem
                                PietSharedStateItem.parseFrom(
                                        sharedState1.getPietSharedStateItem().toByteString()))
                        .build();
        assertThat(sharedState1).isNotEqualTo(sharedState2);

        // Initial PietSharedStateItem messages are equal but not the same between the 2 shared
        // states.
        assertThat(sharedState1.getPietSharedStateItem())
                .isEqualTo(sharedState2.getPietSharedStateItem());
        assertThat(sharedState1.getPietSharedStateItem())
                .isNotSameInstanceAs(sharedState2.getPietSharedStateItem());

        StreamStructure operation =
                StreamStructure.newBuilder()
                        .setContentId(mIdGenerators.createFeatureContentId(0))
                        .setOperation(StreamStructure.Operation.UPDATE_OR_APPEND)
                        .build();
        mFakeStore.setSharedStates(sharedState1, sharedState2)
                .setStreamStructures(HEAD_SESSION_ID, operation);

        ContentId contentId1 = SHARED_STATE_ID.toBuilder().setId(1).build();
        ContentId contentId2 = SHARED_STATE_ID.toBuilder().setId(2).build();
        mFakeProtocolAdapter.addContentId("shared-state-1", contentId1)
                .addContentId("shared-state-2", contentId2);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(mConfiguration);
        assertThat(sessionManager.mInitialized.get()).isFalse();
        sessionManager.initialize();
        assertThat(sessionManager.mInitialized.get()).isTrue();
        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(2);

        StreamSharedState cachedSharedState1 = sessionManager.getSharedState(contentId1);
        StreamSharedState cachedSharedState2 = sessionManager.getSharedState(contentId2);
        assertThat(cachedSharedState1).isEqualTo(sharedState1);
        assertThat(cachedSharedState2).isEqualTo(sharedState2);

        // Cached PietSharedStateItem messages the same between the 2 shared states (memoized).
        assertThat(cachedSharedState1.getPietSharedStateItem())
                .isSameInstanceAs(cachedSharedState2.getPietSharedStateItem());
    }

    @Test
    public void testLifecycleInitialization() {
        FeedSessionManagerImpl sessionManager = createFeedSessionManager(mConfiguration);
        assertThat(sessionManager.mInitialized.get()).isFalse();
        sessionManager.onLifecycleEvent(LifecycleEvent.INITIALIZE);
        assertThat(sessionManager.mInitialized.get()).isTrue();
        sessionManager.onLifecycleEvent(LifecycleEvent.INITIALIZE);
        assertThat(sessionManager.mInitialized.get()).isTrue();
    }

    @Test
    public void testSessionWithContent() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        int featureCnt = 3;
        populateSession(sessionManager, featureCnt, 1, true, null);

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getRootFeature()).isNotNull();

        ModelCursor cursor = modelProvider.getRootFeature().getCursor();
        int cursorCount = 0;
        while (cursor.getNextItem() != null) {
            cursorCount++;
        }
        assertThat(cursorCount).isEqualTo(featureCnt);

        // append a couple of others
        populateSession(sessionManager, featureCnt, featureCnt + 1, false, null);

        cursor = modelProvider.getRootFeature().getCursor();
        cursorCount = 0;
        while (cursor.getNextItem() != null) {
            cursorCount++;
        }
        assertThat(cursorCount).isEqualTo(featureCnt * 2);
    }

    @Test
    public void testNoRequestWithContent_populateIsImmediate() {
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.NO_REQUEST_WITH_CONTENT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        mFakeTaskQueue.resetCounts();

        // Population will happen in an immediate task and no request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.getUserFacingTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.isMakingRequest()).isFalse();
    }

    @Test
    public void testRequestWithContent_populateIsImmediate() {
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        mFakeTaskQueue.resetCounts();

        // Population will happen immediately and a request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.getUserFacingTaskCount()).isEqualTo(1);
        assertThat(mFakeTaskQueue.isMakingRequest()).isTrue();
    }

    @Test
    public void testRequestWithWait_populateIsUserFacing() {
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_WAIT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        mFakeTaskQueue.resetCounts();

        // Population will happen in a user-facing task and a request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeTaskQueue.getImmediateTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.getUserFacingTaskCount()).isEqualTo(2);
        assertThat(mFakeTaskQueue.isMakingRequest()).isTrue();
    }

    @Test
    public void testGetExistingSession_populateIsImmediate() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager,
                /* featureCnt= */ 2,
                /* idStart= */ 1,
                /* reset= */ true,
                /* sharedStateId= */ null);
        ModelProvider modelProvider = getModelProvider(sessionManager);
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();
        mFakeTaskQueue.resetCounts();

        // Population will happen in an immediate task.
        modelProvider = getModelProvider(sessionManager, sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(mFakeTaskQueue.getUserFacingTaskCount()).isEqualTo(0);
    }

    @Test
    public void testMissingFeaturesBeyondThreshold_switchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, STORAGE_MISS_THRESHOLD, 1, true, null);
        mFakeStore.clearContent();

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
        assertThat(mFakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.STORAGE_MISS_BEYOND_THRESHOLD);
    }

    @Test
    public void testMissingFeaturesAtThreshold_doesNotSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, STORAGE_MISS_THRESHOLD - 1, 1, true, null);
        mFakeStore.clearContent();

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(mFakeStore.isEphemeralMode()).isFalse();
        assertThat(mFakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.CONTENT_STORAGE_MISSING_ITEM);
    }

    @Test
    public void testNoCardsError() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.getUpdateConsumer(EMPTY_MUTATION).accept(Result.failure());

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider.getRootFeature()).isNull();

        // Verify the failed session is correct
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).hasSize(1);
        Session session = sessionCache.getAttached(modelProvider.getSessionId());
        assertThat(session).isNotNull();
    }

    @Test
    public void testNoCardsError_populatedHeadSuppressesError() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager,
                /* featureCnt= */ 2,
                /* idStart= */ 1,
                /* reset= */ true,
                /* sharedStateId= */ null);
        sessionManager.getUpdateConsumer(EMPTY_MUTATION).accept(Result.failure());

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider.getRootFeature()).isNotNull();

        // Verify the failed session is correct
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).hasSize(1);
        Session session = sessionCache.getAttached(modelProvider.getSessionId());
        assertThat(session).isNotNull();
    }

    @Test
    public void testModelErrorObserver() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        // verify this runs.  Another method that can'be be verified on a single thread since
        // the noCardsError will be set and unset.
        sessionManager.modelErrorObserver(null, new ModelError(ErrorType.NO_CARDS_ERROR, null));
    }

    @Test
    public void testReset() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        int featureCnt = 3;
        int fullFeatureCount = populateSession(sessionManager, featureCnt, 1, true, null);
        assertThat(fullFeatureCount).isEqualTo(featureCnt + 1);

        fullFeatureCount = populateSession(sessionManager, featureCnt, 1, true, null);
        assertThat(fullFeatureCount).isEqualTo(featureCnt + 1);
    }

    @Test
    public void testHandleToken() {
        ByteString bytes = ByteString.copyFrom("continuation", Charset.defaultCharset());
        StreamToken streamToken = StreamToken.newBuilder()
                                          .setNextPageToken(bytes)
                                          .setParentId(mRootContentId)
                                          .build();
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.handleToken(SESSION_ID, streamToken);

        assertThat(mFakeRequestManager.getLatestStreamToken()).isEqualTo(streamToken);
        assertThat(mFakeStore.getContentById(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID))
                .hasSize(mUploadingActionsEnabled ? 1 : 0);
    }

    @Test
    public void testForceRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.triggerRefresh(
                SESSION_ID, RequestReason.ZERO_STATE, UiContext.getDefaultInstance());

        assertThat(mFakeRequestManager.getLatestRequestReason())
                .isEqualTo(RequestReason.ZERO_STATE);
    }

    @Test
    public void testForceRefresh_scheduledRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.triggerRefresh(
                SESSION_ID, RequestReason.HOST_REQUESTED, UiContext.getDefaultInstance());

        assertThat(mFakeRequestManager.getLatestRequestReason())
                .isEqualTo(RequestReason.HOST_REQUESTED);
    }

    @Test
    public void testGetSharedState() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        String sharedStateId = mIdGenerators.createSharedStateContentId(0);
        ContentId undefinedSharedStateId = ContentId.newBuilder()
                                                   .setContentDomain("shared-state")
                                                   .setId(5)
                                                   .setTable("shared-states")
                                                   .build();
        String undefinedStreamSharedStateId =
                mIdGenerators.createSharedStateContentId(undefinedSharedStateId.getId());
        mFakeProtocolAdapter.addContentId(sharedStateId, SHARED_STATE_ID)
                .addContentId(undefinedStreamSharedStateId, undefinedSharedStateId);

        populateSession(sessionManager, 3, 1, true, sharedStateId);
        assertThat(sessionManager.getSharedState(SHARED_STATE_ID)).isNotNull();

        // test the null condition
        assertThat(sessionManager.getSharedState(undefinedSharedStateId)).isNull();
    }

    @Test
    public void testUpdateConsumer() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(updateConsumer).isInstanceOf(SessionMutationTracker.class);
        assertThat(sessionManager.mOutstandingMutations).hasSize(1);
        assertThat(sessionManager.mOutstandingMutations).contains(updateConsumer);
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
    }

    @Test
    public void testUpdateConsumer_clearAll() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(sessionManager.mOutstandingMutations).hasSize(1);
        mAppLifecycleListener.onClearAll();
        assertThat(sessionManager.mOutstandingMutations).isEmpty();

        // verify this still runs (as a noop)
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
    }

    @Test
    public void testUpdateConsumer_clearAllWithRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(sessionManager.mOutstandingMutations).hasSize(1);
        mAppLifecycleListener.onClearAllWithRefresh();
        assertThat(sessionManager.mOutstandingMutations).isEmpty();

        // verify this still runs (as a noop)
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.mOutstandingMutations).isEmpty();
    }

    @Test
    public void testEdit_semanticProperties() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();

        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");
        StreamDataOperation streamDataOperation =
                StreamDataOperation.newBuilder()
                        .setStreamPayload(StreamPayload.newBuilder().setSemanticData(semanticData))
                        .setStreamStructure(StreamStructure.newBuilder()
                                                    .setContentId(mRootContentId)
                                                    .setOperation(Operation.UPDATE_OR_APPEND))
                        .build();

        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        Result<Model> result = Result.success(Model.of(ImmutableList.of(streamDataOperation)));
        updateConsumer.accept(result);

        assertThat(mFakeStore.getContentById(mRootContentId))
                .contains(new SemanticPropertiesWithId(mRootContentId, semanticData.toByteArray()));
    }

    @Test
    public void testSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        mFakeThreadUtils.enforceMainThread(false);
        sessionManager.switchToEphemeralMode("An Error Message");
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testOnSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        String sharedStateId = mIdGenerators.createSharedStateContentId(0);

        int featureCount = 3;
        populateSession(sessionManager, featureCount, 1, true, sharedStateId);

        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(1);
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        Session session = sessionCache.getHead();
        assertThat(session).isNotNull();
        assertThat(session.getContentInSession()).hasSize(featureCount + 1);

        mFakeThreadUtils.enforceMainThread(false);
        sessionManager.onSwitchToEphemeralMode();

        assertThat(sessionManager.getSharedStateCacheForTest()).isEmpty();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        session = sessionCache.getHead();
        assertThat(session).isNotNull();
        assertThat(session.getContentInSession()).isEmpty();
    }

    @Test
    public void testErrors_initializationSharedStateError() {
        mFakeStore.setAllowGetSharedStates(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_initializationStreamStructureError() {
        mFakeStore.setAllowGetStreamStructures(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_createNewSessionError() {
        mFakeStore.setAllowCreateNewSession(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        populateSession(sessionManager, 5, 1, true, null);

        ModelProvider unused = getModelProvider(sessionManager);
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_getStreamStructuresError() {
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        mFakeStore.setAllowGetStreamStructures(false);
        populateSession(sessionManager, 5, 1, true, null);

        ModelProvider unused = getModelProvider(sessionManager);
        assertThat(mFakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testTriggerUploadActions() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager(
                new Configuration.Builder().put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, true).build());
        ImmutableSet<StreamUploadableAction> actionSet =
                ImmutableSet.of(StreamUploadableAction.getDefaultInstance());
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        mFakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        sessionManager.triggerUploadActions(actionSet);
        assertThat(mFakeActionUploadRequestManager.getLatestActions())
                .containsExactlyElementsIn(actionSet);
    }

    @Test
    public void testGetConsistencyToken() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager(
                new Configuration.Builder().put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, true).build());
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        mFakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        assertThat(sessionManager.getConsistencyToken()).isEqualTo(token);
    }

    @Test
    public void testGetConsistencyTokenEmpty() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        mFakeThreadUtils.enforceMainThread(false);
        assertThat(sessionManager.getConsistencyToken())
                .isEqualTo(ConsistencyToken.getDefaultInstance());
    }

    @Test
    public void testFetchActionsAndUpload() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        ConsistencyToken expectedToken =
                mUploadingActionsEnabled ? token : ConsistencyToken.getDefaultInstance();
        Consumer<Result<ConsistencyToken>> consumer = result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue()).isEqualTo(expectedToken);
        };
        mFakeActionUploadRequestManager.setResult(Result.success(token));
        mFakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        sessionManager.fetchActionsAndUpload(consumer);
        assertThat(mFakeActionUploadRequestManager.getLatestActions()).isNotNull();
    }

    @Test
    public void testStreamSharedStateInterner() {
        Interner<StreamSharedState> interner = new StreamSharedStateInterner();
        StreamSharedState first =
                StreamSharedState.newBuilder()
                        .setContentId("foo")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("equal"))))
                        .build();
        StreamSharedState second =
                StreamSharedState.newBuilder()
                        .setContentId("baz")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("equal"))))
                        .build();
        StreamSharedState third =
                StreamSharedState.newBuilder()
                        .setContentId("bar")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("different"))))
                        .build();
        assertThat(first).isNotSameInstanceAs(second);
        assertThat(first.getPietSharedStateItem()).isEqualTo(second.getPietSharedStateItem());
        assertThat(first).isNotEqualTo(third);
        assertThat(first.getPietSharedStateItem()).isNotEqualTo(third.getPietSharedStateItem());

        // Pool is empty so first is added/returned.
        StreamSharedState internedFirst = interner.intern(first);
        assertThat(interner.size()).isEqualTo(1);
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has an identical inner PietSharedStateItem proto, which is used.
        StreamSharedState internedSecond = interner.intern(second);
        assertThat(interner.size()).isEqualTo(1);
        // The returned proto is equal to second, but its internal PietSharedStateItem is the same
        // as the one in first (memoized).
        assertThat(internedSecond).isNotSameInstanceAs(second);
        assertThat(internedSecond).isEqualTo(second);
        assertThat(internedSecond.getPietSharedStateItem())
                .isSameInstanceAs(first.getPietSharedStateItem());

        // Third has a new PietSharedStateItem (not equal with any previous) so it is added to the
        // pool.
        StreamSharedState internedThird = interner.intern(third);
        assertThat(interner.size()).isEqualTo(2);
        assertThat(internedThird).isSameInstanceAs(third);
    }

    @Test
    public void testGetNewSession() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();

        UiContext uiContext = UiContext.newBuilder()
                                      .setExtension(UiContextForTest.uiContextForTest,
                                              UiContextForTest.newBuilder().setValue(3).build())
                                      .build();

        ModelProvider modelProvider = getModelProvider(sessionManager, uiContext);

        sessionManager.getNewSession(modelProvider, /* viewDepthProvider= */ null, uiContext);
        ModelProviderObserver modelProviderObserver = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(modelProviderObserver);

        verify(modelProviderObserver).onSessionStart(uiContext);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void testLifecycleEventsWhenConditionalUploadFeatureEnabled() {
        when(mPrefService.getBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS))
                .thenReturn(false);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(mConfiguration);

        sessionManager.onLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
        sessionManager.onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
        sessionManager.onLifecycleEvent(LifecycleEvent.SIGNED_IN);
        sessionManager.onLifecycleEvent(LifecycleEvent.SIGNED_OUT);

        verify(mActionManager, times(4)).setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void testLifecycleEventsWhenConditionalUploadFeatureDisabled() {
        when(mPrefService.getBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS))
                .thenReturn(false);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(mConfiguration);

        sessionManager.onLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
        sessionManager.onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
        sessionManager.onLifecycleEvent(LifecycleEvent.SIGNED_IN);
        sessionManager.onLifecycleEvent(LifecycleEvent.SIGNED_OUT);

        verify(mActionManager, times(4)).setCanUploadClicksAndViewsWhenNoticeCardIsPresent(true);
    }

    private int populateSession(FeedSessionManagerImpl sessionManager, int featureCnt, int idStart,
            boolean reset,
            /*@Nullable*/ String sharedStateId) {
        int operationCount = 0;

        InternalProtocolBuilder internalProtocolBuilder = new InternalProtocolBuilder();
        if (reset) {
            internalProtocolBuilder.addClearOperation().addRootFeature();
            operationCount++;
        }
        for (int i = 0; i < featureCnt; i++) {
            internalProtocolBuilder.addFeature(
                    mContentIdGenerators.createFeatureContentId(idStart++),
                    mIdGenerators.createRootContentId(0));
            operationCount++;
        }
        if (sharedStateId != null) {
            internalProtocolBuilder.addSharedState(sharedStateId);
            operationCount++;
        }
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        updateConsumer.accept(Result.success(Model.of(internalProtocolBuilder.build())));
        return operationCount;
    }

    private ModelProvider getModelProvider(FeedSessionManager sessionManager) {
        return getModelProvider(
                sessionManager, /* sessionId= */ null, UiContext.getDefaultInstance());
    }

    private ModelProvider getModelProvider(FeedSessionManager sessionManager, UiContext uiContext) {
        return getModelProvider(sessionManager, /* sessionId= */ null, uiContext);
    }

    private ModelProvider getModelProvider(
            FeedSessionManager sessionManager, String sessionId, UiContext uiContext) {
        ModelProviderFactory modelProviderFactory = new FeedModelProviderFactory(sessionManager,
                mFakeThreadUtils, mTimingUtils, mFakeTaskQueue, mFakeMainThreadRunner,
                mConfiguration, mFakeBasicLoggingApi);
        if (sessionId == null) {
            return modelProviderFactory.createNew(/* viewDepthProvider= */ null, uiContext);
        } else {
            return modelProviderFactory.create(sessionId, uiContext);
        }
    }

    private FeedSessionManagerImpl getInitializedSessionManager() {
        return getInitializedSessionManager(mConfiguration);
    }

    private FeedSessionManagerImpl getInitializedSessionManager(Configuration config) {
        FeedSessionManagerImpl fsm = createFeedSessionManager(config);
        fsm.initialize();
        return fsm;
    }

    private FeedSessionManagerImpl getUninitializedSessionManager() {
        return createFeedSessionManager(mConfiguration);
    }

    private FeedSessionManagerImpl createFeedSessionManager(Configuration configuration) {
        return new FeedSessionManagerFactory(mFakeTaskQueue, mFakeStore, mTimingUtils,
                mFakeThreadUtils, mFakeProtocolAdapter, mFakeRequestManager,
                mFakeActionUploadRequestManager, mSchedulerApi, configuration, mFakeClock,
                mAppLifecycleListener, mFakeMainThreadRunner, mFakeBasicLoggingApi, mActionManager)
                .create();
    }
}
