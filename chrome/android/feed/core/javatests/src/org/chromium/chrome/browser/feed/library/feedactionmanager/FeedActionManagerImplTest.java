// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.feedactionmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.v1.FeedLoggingBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.time.Duration;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

/** Tests of the {@link FeedActionManagerImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures({ChromeFeatureList.INTEREST_FEED_V2,
        ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD})
@Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
        ChromeFeatureList.REPORT_FEED_USER_ACTIONS})
public class FeedActionManagerImplTest {
    private static final String CONTENT_ID_STRING = "contentIdString";
    private static final String SESSION_ID = "session";
    private static final long DEFAULT_TIME = Duration.ofDays(42).toMillis();
    private static final long DEFAULT_TIME_SECONDS = Duration.ofDays(42).getSeconds();
    // View dimensions on screen. We satisfy or violate exposure and coverage (see
    // FeedActionManagerImpl.VIEW_EXPOSURE_THRESHOLD and
    // FeedActionManagerImpl.VIEWPORT_COVERAGE_THRESHOLD for definitions and values). Dimensions are
    // chosen according to the default exposure and coverage thresholds, and the size of the
    // viewport.
    private static final Rect VIEWPORT_RECT = new Rect(0, 0, 100, 100);
    private static final Rect VISIBLE_RECT = new Rect(0, 0, 100, 50);
    private static final Rect INVISIBLE_RECT = new Rect(0, -50, 100, 10);
    // Durations for VIEW actions. Chosen according to the default VIEW duration threshold
    // FeedActionManagerImpl.VIEW_DURATION_MS_THRESHOLD_DEFAULT.
    private static final long LONG_DURATION_MS = 1000;
    private static final long LONG_DURATION_S = Duration.ofMillis(LONG_DURATION_MS).toSeconds();
    private static final long SHORT_DURATION_MS = 400;
    private static final ActionPayload ACTION_PAYLOAD = ActionPayload.getDefaultInstance();

    private final FakeClock mFakeClock = new FakeClock();
    private final FakeMainThreadRunner mFakeMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Store mStore;
    @Mock
    private LocalActionMutation mLocalActionMutation;
    @Mock
    private UploadableActionMutation mUploadableActionMutation;
    @Mock
    private Consumer<Result<Model>> mModelConsumer;
    @Mock
    private FeedLoggingBridge mFeedLoggingBridge;
    @Mock
    private Runnable mStoreViewActionsRunnable;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;

    @Captor
    private ArgumentCaptor<Integer> mActionTypeCaptor;
    @Captor
    private ArgumentCaptor<String> mContentIdStringCaptor;
    @Captor
    private ArgumentCaptor<Result<Model>> mModelCaptor;
    @Captor
    private ArgumentCaptor<MutationContext> mMutationContextCaptor;
    @Captor
    private ArgumentCaptor<Consumer<Result<ConsistencyToken>>> mConsumerCaptor;
    @Captor
    private ArgumentCaptor<Set<StreamUploadableAction>> mActionCaptor;
    @Captor
    private ArgumentCaptor<StreamUploadableAction> mUploadableActionCaptor;

    private FeedActionManagerImpl mActionManager;

    private TestView mViewport;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        mActionManager = new FeedActionManagerImpl(mStore, mFakeThreadUtils, getTaskQueue(),
                mFakeMainThreadRunner, new TestViewHandler(), mFakeClock, mFeedLoggingBridge);
        mActionManager.initialize(mFeedSessionManager);

        when(mFeedSessionManager.getUpdateConsumer(any(MutationContext.class)))
                .thenReturn(mModelConsumer);

        when(mLocalActionMutation.add(anyInt(), anyString())).thenReturn(mLocalActionMutation);
        when(mStore.editLocalActions()).thenReturn(mLocalActionMutation);

        when(mUploadableActionMutation.upsert(any(StreamUploadableAction.class), anyString()))
                .thenReturn(mUploadableActionMutation);
        when(mStore.editUploadableActions()).thenReturn(mUploadableActionMutation);

        mFakeClock.set(DEFAULT_TIME);
        mViewport = new TestView();
        mViewport.setRectOnScreen(VIEWPORT_RECT);
        mActionManager.setViewport(mViewport);
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
    }

    @Test
    public void dismissLocal() throws Exception {
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), null);

        verify(mLocalActionMutation)
                .add(mActionTypeCaptor.capture(), mContentIdStringCaptor.capture());
        assertThat(mActionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(mContentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(mLocalActionMutation).commit();

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismissLocal_sessionIdSet() throws Exception {
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), SESSION_ID);

        verify(mLocalActionMutation)
                .add(mActionTypeCaptor.capture(), mContentIdStringCaptor.capture());
        assertThat(mActionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(mContentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(mLocalActionMutation).commit();

        verify(mFeedSessionManager).getUpdateConsumer(mMutationContextCaptor.capture());
        assertThat(mMutationContextCaptor.getValue().getRequestingSessionId())
                .isEqualTo(SESSION_ID);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismiss() throws Exception {
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismiss(Collections.singletonList(dataOperation), null);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismiss_sessionIdSet() throws Exception {
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismiss(Collections.singletonList(dataOperation), SESSION_ID);

        verify(mFeedSessionManager).getUpdateConsumer(mMutationContextCaptor.capture());
        assertThat(mMutationContextCaptor.getValue().getRequestingSessionId())
                .isEqualTo(SESSION_ID);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void triggerCreateAndUploadAction_whenUploadDisabled_byCantUploadWithNotice()
            throws Exception {
        // Set things so that, when the conditional upload feature is enabled, the upload of clicks
        // and views cannot take place.
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(
                CONTENT_ID_STRING, ACTION_PAYLOAD, ActionManager.UploadActionType.CLICK);
        verify(mFeedSessionManager, never()).triggerUploadActions(mActionCaptor.capture());

        triggerViewActionAndVerifyUpserted(/* expectUpserted= */ false);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void triggerCreateAndUploadAction_whenUploadEnabled_byCanUploadWithNotice()
            throws Exception {
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(true);
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(
                CONTENT_ID_STRING, ACTION_PAYLOAD, ActionManager.UploadActionType.CLICK);
        verify(mFeedSessionManager).triggerUploadActions(mActionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) mActionCaptor.getValue().toArray()[0];
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID_STRING);
        assertThat(action.getTimestampSeconds()).isEqualTo(DEFAULT_TIME_SECONDS);
        assertThat(action.getPayload()).isEqualTo(ACTION_PAYLOAD);

        triggerViewActionAndVerifyUpserted(/* expectUpserted= */ true);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void triggerCreateAndUploadAction_whenUploadEnabled_byNoClickAction() throws Exception {
        // Set things so that, when the conditional upload feature is enabled, the upload of clicks
        // and views cannot take place.
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(
                CONTENT_ID_STRING, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
        verify(mFeedSessionManager).triggerUploadActions(mActionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) mActionCaptor.getValue().toArray()[0];
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID_STRING);
        assertThat(action.getTimestampSeconds()).isEqualTo(DEFAULT_TIME_SECONDS);
        assertThat(action.getPayload()).isEqualTo(ACTION_PAYLOAD);

        // Try to store a view action and verify it isn't stored because, althought explicit actions
        // can be uploaded, clicks and views cannot be yet uploaded.
        triggerViewActionAndVerifyUpserted(/* expectUpserted= */ false);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void triggerCreateAndUploadAction_whenLogEnabled_byNoNoticeCard() throws Exception {
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(false);

        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(
                CONTENT_ID_STRING, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
        verify(mFeedSessionManager).triggerUploadActions(mActionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) mActionCaptor.getValue().toArray()[0];
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID_STRING);
        assertThat(action.getTimestampSeconds()).isEqualTo(DEFAULT_TIME_SECONDS);
        assertThat(action.getPayload()).isEqualTo(ACTION_PAYLOAD);

        triggerViewActionAndVerifyUpserted(/* expectUpserted= */ true);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void triggerCreateAndUploadAction_whenLogEnabled_byCondUploadFeatureDisabled()
            throws Exception {
        // Set things so that, when the conditional upload feature is enabled, the upload of clicks
        // and views would not take place.
        mActionManager.setCanUploadClicksAndViewsWhenNoticeCardIsPresent(false);
        when(mPrefService.getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD)).thenReturn(true);

        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(
                CONTENT_ID_STRING, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
        verify(mFeedSessionManager).triggerUploadActions(mActionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) mActionCaptor.getValue().toArray()[0];
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID_STRING);
        assertThat(action.getTimestampSeconds()).isEqualTo(DEFAULT_TIME_SECONDS);
        assertThat(action.getPayload()).isEqualTo(ACTION_PAYLOAD);

        triggerViewActionAndVerifyUpserted(/* expectUpserted= */ true);
    }

    @Test
    public void triggerUploadAllActions() throws Exception {
        String url = "url";
        String param = "param";
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0x2}))
                                         .build();
        String expectedUrl = FeedActionManagerImpl.updateParam(url, param, token.toByteArray());
        Consumer<String> consumer = result -> {
            assertThat(result).isEqualTo(expectedUrl);
        };
        mActionManager.uploadAllActionsAndUpdateUrl(url, param, consumer);
        verify(mFeedSessionManager).fetchActionsAndUpload(mConsumerCaptor.capture());
        mConsumerCaptor.getValue().accept(Result.success(token));
    }

    @Test
    public void onShow_viewTracked_coverage() {
        showThenHide(
                // View covers >50% (FeedActionManagerImpl.VIEWPORT_COVERAGE_THRESHOLD) of viewport,
                // but <50% (FeedActionManagerImpl.VIEW_EXPOSURE_THRESHOLD) of the view is visible.
                new Rect(0, -1000, 100, 51), CONTENT_ID_STRING, LONG_DURATION_MS);
        verifyActionUpserted(ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S, LONG_DURATION_MS);
    }

    @Test
    public void onShow_viewTracked_exposure() {
        showThenHide(
                // >50% (FeedActionManagerImpl.VIEW_EXPOSURE_THRESHOLD) of the view is visible, but
                // it covers <50% (FeedActionManagerImpl.VIEWPORT_COVERAGE_THRESHOLD) of the
                // viewport.
                new Rect(0, -48, 100, 49), CONTENT_ID_STRING, LONG_DURATION_MS);
        verifyActionUpserted(ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S, LONG_DURATION_MS);
    }

    @Test
    public void onShow_viewTracked_noExposure_noCoverage() {
        showThenHide(
                // <50% (FeedActionManagerImpl.VIEW_EXPOSURE_THRESHOLD) of the view is visible and
                // it covers <50% (FeedActionManagerImpl.VIEWPORT_COVERAGE_THRESHOLD) of the
                // viewport.
                new Rect(0, -50, 100, 49), CONTENT_ID_STRING, LONG_DURATION_MS);
        verifyNoActionUpserted();
    }

    @Test
    public void onShow_viewTracked_visible_tooShort() {
        showThenHide(VISIBLE_RECT, CONTENT_ID_STRING, SHORT_DURATION_MS);
        verifyNoActionUpserted();
    }

    @Test
    public void onShow_viewTracked_visible_repeated_tooShort() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(SHORT_DURATION_MS);
        mActionManager.onHide();

        mFakeClock.advance(1000);

        mActionManager.onShow();
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(SHORT_DURATION_MS);
        mActionManager.onHide();

        verifyNoActionUpserted();
    }

    @Test
    public void onShow_viewTracked_visible_repeated() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();

        mFakeClock.advance(1000);

        mActionManager.onShow();
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();

        verifyActionsUpserted(StreamUploadableAction.newBuilder()
                                      .setFeatureContentId(CONTENT_ID_STRING)
                                      .setPayload(ACTION_PAYLOAD)
                                      .setTimestampSeconds(DEFAULT_TIME_SECONDS + LONG_DURATION_S)
                                      .setDurationMs(LONG_DURATION_MS)
                                      .build(),
                StreamUploadableAction.newBuilder()
                        .setFeatureContentId(CONTENT_ID_STRING)
                        .setPayload(ACTION_PAYLOAD)
                        .setTimestampSeconds(
                                DEFAULT_TIME_SECONDS + LONG_DURATION_S + 1 + LONG_DURATION_S)
                        .setDurationMs(LONG_DURATION_MS)
                        .build());
    }

    @Features.DisableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    @Test
    public void onShow_viewTrackedVisible_featureDisabled() {
        showThenHide(VISIBLE_RECT, CONTENT_ID_STRING, LONG_DURATION_MS);
        verifyNoActionUpserted();
    }

    @Test
    public void onShow_viewNotTracked() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();
        verifyNoActionUpserted();
    }

    @Test
    public void onShow_multipleViewsTracked() {
        TestView view1 = new TestView();
        TestView parentview = new TestView();
        TestView view2 = new TestView();
        TestView view3 = new TestView();
        TestView view4 = new TestView();

        mActionManager.onShow();

        // Four views in the viewport, in vertical sequence, all visible except the last one.
        view1.setRectOnScreen(new Rect(0, 0, 100, 20));
        mViewport.children.add(view1);
        view2.setRectOnScreen(new Rect(0, 30, 100, 50));
        view3.setRectOnScreen(new Rect(0, 60, 100, 80));
        view4.setRectOnScreen(new Rect(0, 90, 100, 1000));
        parentview.children.add(view2);
        parentview.children.add(view3);
        parentview.children.add(view4);
        mViewport.children.add(parentview);

        // All views tracked except the second one.
        mActionManager.onViewVisible(view1, "contentId1", ACTION_PAYLOAD);
        mActionManager.onViewVisible(view3, "contentId3", ACTION_PAYLOAD);
        mActionManager.onViewVisible(view4, "contentId4", ACTION_PAYLOAD);

        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();

        verifyActionsUpserted(StreamUploadableAction.newBuilder()
                                      .setFeatureContentId("contentId1")
                                      .setPayload(ACTION_PAYLOAD)
                                      .setTimestampSeconds(DEFAULT_TIME_SECONDS + LONG_DURATION_S)
                                      .setDurationMs(LONG_DURATION_MS)
                                      .build(),
                StreamUploadableAction.newBuilder()
                        .setFeatureContentId("contentId3")
                        .setPayload(ACTION_PAYLOAD)
                        .setTimestampSeconds(DEFAULT_TIME_SECONDS + LONG_DURATION_S)
                        .setDurationMs(LONG_DURATION_MS)
                        .build());
    }

    private void showThenHide(Rect viewRect, String contentId, long preHideDurationMs) {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(viewRect);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, contentId, ACTION_PAYLOAD);
        mFakeClock.advance(preHideDurationMs);
        mActionManager.onHide();
    }

    @Test
    public void onScroll_visible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(INVISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mActionManager.onScrollStart();
        view.setRectOnScreen(VISIBLE_RECT);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onScrollEnd();
        mFakeClock.advance(1000);
        mActionManager.onHide();
        verifyActionUpserted(
                ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S + 1, LONG_DURATION_MS);
    }

    @Test
    public void onScroll_notVisible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mActionManager.onScrollStart();
        view.setRectOnScreen(INVISIBLE_RECT);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onScrollEnd();
        mActionManager.onHide();
        verifyNoActionUpserted();
    }

    @Test
    public void onAnimationFinished_visible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(INVISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        view.setRectOnScreen(VISIBLE_RECT);
        mActionManager.onAnimationFinished();
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();
        verifyActionUpserted(ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S, LONG_DURATION_MS);
    }

    @Test
    public void onAnimationFinished_notVisible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        view.setRectOnScreen(INVISIBLE_RECT);
        mActionManager.onAnimationFinished();
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();
        verifyNoActionUpserted();
    }

    @Test
    public void onLayoutChange_visible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(INVISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        view.setRectOnScreen(VISIBLE_RECT);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onLayoutChange();
        mFakeClock.advance(1000);
        mActionManager.onHide();
        verifyActionUpserted(
                ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S + 1, LONG_DURATION_MS);
    }

    @Test
    public void onLayoutChange_notVisible() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        view.setRectOnScreen(INVISIBLE_RECT);
        mActionManager.onLayoutChange();
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();
        verifyNoActionUpserted();
    }

    @Test
    public void storeViewActions() {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onLayoutChange();
        verifyNoActionUpserted();
        mActionManager.storeViewActions(() -> {
            verifyActionUpserted(
                    ACTION_PAYLOAD, CONTENT_ID_STRING, LONG_DURATION_S, LONG_DURATION_MS);
            mStoreViewActionsRunnable.run();
        });
        verify(mStoreViewActionsRunnable).run();
    }

    @Test
    public void storeViewActions_calledbackNoActions() {
        mActionManager.storeViewActions(() -> {
            verifyNoActionUpserted();
            mStoreViewActionsRunnable.run();
        });
        verify(mStoreViewActionsRunnable, times(1)).run();
    }

    private void verifyActionUpserted(
            ActionPayload payload, String contentId, long elapsedTimeS, long durationMs) {
        verifyActionsUpserted(StreamUploadableAction.newBuilder()
                                      .setFeatureContentId(contentId)
                                      .setPayload(payload)
                                      .setTimestampSeconds(DEFAULT_TIME_SECONDS + elapsedTimeS)
                                      .setDurationMs(durationMs)
                                      .build());
    }

    private void verifyActionsUpserted(StreamUploadableAction... actions) {
        verify(mUploadableActionMutation, times(actions.length))
                .upsert(mUploadableActionCaptor.capture(), mContentIdStringCaptor.capture());
        assertThat(mUploadableActionCaptor.getAllValues())
                .containsExactlyElementsIn(Arrays.asList(actions));
        List<String> contentIds = new LinkedList<>();
        for (StreamUploadableAction action : actions) {
            contentIds.add(action.getFeatureContentId());
        }
        assertThat(mContentIdStringCaptor.getAllValues()).containsExactlyElementsIn(contentIds);
    }

    private void verifyNoActionUpserted() {
        verify(mUploadableActionMutation, never())
                .upsert(any(StreamUploadableAction.class), anyString());
    }

    private StreamDataOperation buildBasicDismissOperation() {
        return StreamDataOperation.newBuilder()
                .setStreamStructure(StreamStructure.newBuilder()
                                            .setContentId(CONTENT_ID_STRING)
                                            .setOperation(Operation.REMOVE))
                .build();
    }

    private void setUpDismissMocks() {
        when(mFeedSessionManager.getUpdateConsumer(any(MutationContext.class)))
                .thenReturn(mModelConsumer);
        when(mLocalActionMutation.add(anyInt(), anyString())).thenReturn(mLocalActionMutation);
        when(mStore.editLocalActions()).thenReturn(mLocalActionMutation);
    }

    private void setupCreateAndStoreMocks() {
        when(mUploadableActionMutation.upsert(any(StreamUploadableAction.class), anyString()))
                .thenReturn(mUploadableActionMutation);
        when(mStore.editUploadableActions()).thenReturn(mUploadableActionMutation);
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        return fakeTaskQueue;
    }

    private class TestView extends View {
        private Rect mRectOnScreen;
        public final List<View> children = new LinkedList<>();

        public TestView() {
            super(Robolectric.buildActivity(Activity.class).get());
            mRectOnScreen = new Rect(0, 0, 0, 0);
        }

        public void setRectOnScreen(Rect rect) {
            mRectOnScreen = new Rect(rect);
        }

        public Rect getRectOnScreen() {
            return new Rect(mRectOnScreen);
        }
    }

    private class TestViewHandler extends FeedActionManagerImpl.ViewHandler {
        @Override
        public int getChildCount(View view) {
            return ((TestView) view).children.size();
        }

        @Override
        public View getChildAt(View view, int index) {
            return ((TestView) view).children.get(index);
        }

        @Override
        public Rect getRectOnScreen(View view) {
            return ((TestView) view).getRectOnScreen();
        }
    }

    private void triggerViewActionAndVerifyUpserted(boolean expectUpserted) {
        mActionManager.onShow();
        TestView view = new TestView();
        view.setRectOnScreen(VISIBLE_RECT);
        mViewport.children.add(view);
        mActionManager.onViewVisible(view, CONTENT_ID_STRING, ACTION_PAYLOAD);
        mFakeClock.advance(LONG_DURATION_MS);
        mActionManager.onHide();

        if (expectUpserted) {
            verifyActionsUpserted(
                    StreamUploadableAction.newBuilder()
                            .setFeatureContentId(CONTENT_ID_STRING)
                            .setPayload(ACTION_PAYLOAD)
                            .setTimestampSeconds(DEFAULT_TIME_SECONDS + LONG_DURATION_S)
                            .setDurationMs(LONG_DURATION_MS)
                            .build());
        } else {
            verify(mUploadableActionMutation, never())
                    .upsert(mUploadableActionCaptor.capture(), mContentIdStringCaptor.capture());
        }
    }
}
