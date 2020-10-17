// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.shared.stream.Stream.POSITION_NOT_KNOWN;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.app.Activity;
import android.content.Context;
import android.util.Base64;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Consumer;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.ZeroStateShowReason;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.basicstream.internal.StreamItemAnimator;
import org.chromium.chrome.browser.feed.library.basicstream.internal.StreamRecyclerViewAdapter;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.StreamDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.BasicStreamScrollMonitor;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.ScrollRestorer;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.PietManager;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.sharedstream.contentchanged.StreamContentChangedListener;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManagerImpl;
import org.chromium.chrome.browser.feed.library.sharedstream.deepestcontenttracker.DeepestContentTracker;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietEventLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollListenerNotifier;
import org.chromium.chrome.browser.feed.library.testing.shadows.ShadowRecycledViewPool;
import org.chromium.chrome.browser.feed.shared.stream.Header;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.libraries.basicstream.internal.StreamSavedInstanceStateProto.StreamSavedInstanceState;
import org.chromium.components.feed.core.proto.libraries.sharedstream.ScrollStateProto.ScrollState;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason.Reason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for {@link BasicStream}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecycledViewPool.class})
@Features.DisableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
public class BasicStreamTest {
    private static final int START_PADDING = 1;
    private static final int END_PADDING = 2;
    private static final int TOP_PADDING = 3;
    private static final int BOTTOM_PADDING = 4;
    private static final long LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS = 1000;
    private static final int ADAPTER_HEADER_COUNT = 5;

    private static final String SESSION_ID = "session-id";
    private static final ScrollState SCROLL_STATE =
            ScrollState.newBuilder().setOffset(10).setPosition(10).build();
    private static final StreamSavedInstanceState SAVED_INSTANCE_STATE =
            StreamSavedInstanceState.newBuilder()
                    .setSessionId(SESSION_ID)
                    .setScrollState(SCROLL_STATE)
                    .build();
    private static final long SPINNER_DELAY_MS = 123L;
    private static final long SPINNER_MINIMUM_SHOW_TIME_MS = 655L;
    private static final Configuration CONFIGURATION =
            new Configuration.Builder()
                    .put(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                            LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS)
                    .put(ConfigKey.SPINNER_DELAY_MS, SPINNER_DELAY_MS)
                    .put(ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS, SPINNER_MINIMUM_SHOW_TIME_MS)
                    .build();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private StreamConfiguration mStreamConfiguration;
    @Mock
    private ModelFeature mModelFeature;
    @Mock
    private ModelProviderFactory mModelProviderFactory;
    @Mock
    private ModelProvider mInitialModelProvider;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private ModelProvider mRestoredModelProvider;
    @Mock
    private PietManager mPietManager;
    @Mock
    private SnackbarApi mSnackbarApi;
    @Mock
    private StreamDriver mStreamDriver;
    @Mock
    private StreamRecyclerViewAdapter mAdapter;
    @Mock
    private ScrollListenerNotifier mScrollListenerNotifier;
    @Mock
    private ScrollRestorer mNonRestoringScrollRestorer;
    @Mock
    private ScrollRestorer mScrollRestorer;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private ContextMenuManagerImpl mContextMenuManager;
    @Mock
    private ViewLoggingUpdater mViewLoggingUpdater;
    @Mock
    private TooltipApi mTooltipApi;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;

    private FakeFeedKnownContent mFakeFeedKnownContent;
    private LinearLayoutManagerWithFakePositioning mLayoutManager;
    private Context mContext;
    private FakeClock mClock;
    private BasicStreamForTest mBasicStream;
    private FakeMainThreadRunner mMainThreadRunner;
    private List<Header> mHeaders;

    @Before
    public void setUp() {
        initMocks(this);

        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS))
                .thenReturn(true);

        mFakeFeedKnownContent = new FakeFeedKnownContent();
        mHeaders = new ArrayList<>();
        mHeaders.add(mock(Header.class));

        // TODO: Move header orchestration into separate class.
        // Purposely using a different header count here as it is possible for size of headers to
        // change due to swipe to dismiss.  Adapter is source of truth for headers right now.
        // Ideally we should have a drivers specifically for header management but we don't just
        // yet.
        when(mAdapter.getHeaderCount()).thenReturn(ADAPTER_HEADER_COUNT);

        when(mStreamConfiguration.getPaddingStart()).thenReturn(START_PADDING);
        when(mStreamConfiguration.getPaddingEnd()).thenReturn(END_PADDING);
        when(mStreamConfiguration.getPaddingTop()).thenReturn(TOP_PADDING);
        when(mStreamConfiguration.getPaddingBottom()).thenReturn(BOTTOM_PADDING);

        when(mModelProviderFactory.createNew(any(ViewDepthProvider.class), any(UiContext.class)))
                .thenReturn(mInitialModelProvider, mModelProvider);

        when(mInitialModelProvider.getSessionId()).thenReturn(SESSION_ID);

        when(mScrollRestorer.getScrollStateForScrollRestore(ADAPTER_HEADER_COUNT))
                .thenReturn(SCROLL_STATE);

        when(mStreamDriver.getLeafFeatureDrivers()).thenReturn(Collections.emptyList());

        mContext = Robolectric.buildActivity(Activity.class).get();
        mClock = new FakeClock();
        mMainThreadRunner = FakeMainThreadRunner.create(mClock);
        mLayoutManager = new LinearLayoutManagerWithFakePositioning(mContext);

        mBasicStream = createBasicStream(mLayoutManager);
        mBasicStream.onCreate(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void testOnCreate_setReachedUploadConditionsBitInActionManager_whenFeatureEnabled() {
        verify(mActionManager, times(1)).setCanUploadClicksAndViewsWhenNoticeCardIsPresent(true);
    }

    @Test
    public void testRecyclerViewSetup() {
        assertThat(getStreamRecyclerView().getId()).isEqualTo(R.id.feed_stream_recycler_view);
    }

    @Test
    public void testOnSessionStart() {
        mBasicStream.onShow();
        reset(mAdapter);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mStreamDriver).onDestroy();
        verify(mAdapter).setDriver(mStreamDriver);
        assertThat(mBasicStream.mStreamDriverScrollRestorer).isSameInstanceAs(mScrollRestorer);
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithStreamContentAfterOnShow() {
        mClock = mClock.set(10);
        mBasicStream.onShow();

        when(mStreamDriver.hasContent()).thenReturn(true);
        mClock = mClock.set(40);
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi).onOpenedWithContent(30, 0);
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithStreamContentAfterOnShow_whenRestoring() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        mBasicStream = createBasicStream(new LinearLayoutManager(mContext));

        mBasicStream.onCreate(savedInstanceState);

        mClock.set(15L);
        mBasicStream.onShow();

        when(mStreamDriver.hasContent()).thenReturn(true);
        mClock.advance(5L);
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi).onOpenedWithContent(5, 0);
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithStreamContentAfterInitialOnShow() {
        mBasicStream.onShow();
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(mBasicLoggingApi);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi, never()).onOpenedWithContent(anyInt(), anyInt());
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithStreamContent_IfOnErrorLogsNoContent() {
        mBasicStream.onShow();
        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(mBasicLoggingApi);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi, never()).onOpenedWithContent(anyInt(), anyInt());
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoContent_ifStreamDriverDoesNotHaveContent() {
        mBasicStream.onShow();

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void
    testOnSessionStart_doesNotUseNewStreamDriver_ifBothStreamDriversAreShowingZeroState() {
        StreamDriver newStreamDriver = mock(StreamDriver.class);
        mBasicStream.onShow();
        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(mAdapter, mStreamDriver);
        when(mStreamDriver.isZeroStateBeingShown()).thenReturn(true);
        when(newStreamDriver.isZeroStateBeingShown()).thenReturn(true);

        mBasicStream.mStreamDriver = newStreamDriver;
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mStreamDriver).setModelProviderForZeroState(mInitialModelProvider);
        verify(mStreamDriver, never()).onDestroy();
        verify(newStreamDriver).onDestroy();
        verify(mAdapter, never()).setDriver(any(StreamDriver.class));
    }

    @Test
    public void testOnSessionFinished() {
        mBasicStream.onShow();
        reset(mStreamDriver);
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(mScrollRestorer).abandonRestoringScroll();
        verify(mInitialModelProvider).unregisterObserver(mBasicStream);
        verify(mModelProviderFactory, times(2))
                .createNew(any(ViewDepthProvider.class), any(UiContext.class));
        verify(mModelProvider).registerObserver(mBasicStream);
    }

    @Test
    public void testOnError_showsZeroState() {
        mBasicStream.onShow();
        reset(mStreamDriver);

        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(mScrollRestorer).abandonRestoringScroll();
        verify(mStreamDriver).showZeroState(ZeroStateShowReason.ERROR);
    }

    @Test
    public void testOnError_logsOnOpenedWithNoContent() {
        when(mStreamDriver.hasContent()).thenReturn(false);
        mClock = mClock.set(10);
        mBasicStream.onShow();

        mClock = mClock.set(10 + LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS);
        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(mBasicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void testOnError_doesNotDoubleLogOnOpenedWithNoContent() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(mInitialModelProvider.getRootFeature()).thenReturn(null);

        mBasicStream.onShow();
        // Trigger onOpenedWithNoContent logging through updating the driver.
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(mBasicLoggingApi);

        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(mBasicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoImmediateContent() {
        mBasicStream.onShow();

        // Advance so that the spinner starts showing
        mClock.advance(SPINNER_DELAY_MS);

        // Advance so that is has taken long enough that onOpenedWithNoImmediateContent is logged.
        mClock.advance(LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithNoImmediateContent_ifNotWithinThreshold() {
        mBasicStream.onShow();

        mClock.advance(LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS - 1);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi, never()).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoContent() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(mInitialModelProvider.getRootFeature()).thenReturn(null);
        mBasicStream.onShow();

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void testOnShow_doesNotLogOnOpenedWithNoContent_ifModelProviderNotReady() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.INITIALIZING);
        when(mInitialModelProvider.getRootFeature()).thenReturn(null);

        mBasicStream.onShow();

        verify(mBasicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnShow_doesNotLogOnOpenedWithNoContent_ifRootFeatureNotNull() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(mInitialModelProvider.getRootFeature()).thenReturn(mModelFeature);

        mBasicStream.onShow();

        verify(mBasicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_doesNotDoubleLogOnOpenedWithNoContent() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(mInitialModelProvider.getRootFeature()).thenReturn(null);

        mBasicStream.onShow();
        // Trigger onOpenedWithNoContent logging through onError.
        mBasicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(mBasicLoggingApi);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithNoImmediateContentAfterInitialOnShow() {
        mBasicStream.onShow();
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(mBasicLoggingApi);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mBasicLoggingApi, never()).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnError_doesNotShowZeroState() {
        mBasicStream.onShow();

        assertThatRunnable(
                ()
                        -> mBasicStream.onError(new ModelError(ErrorType.PAGINATION_ERROR,
                                /* continuationToken= */ null)))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testLifecycle_onCreateWithStringCalledOnlyOnce() {
        // onCreate is called once in setup
        assertThatRunnable(() -> mBasicStream.onCreate(""))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testLifecycle_getViewBeforeOnCreateCrashes() {
        // create BasicStream that has not had onCreate() called.
        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        assertThatRunnable(() -> mBasicStream.getView())
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testLifecycle_onCreate_onDestroy() {
        mBasicStream.onDestroy();
        verify(mInitialModelProvider, never()).invalidate();
    }

    @Test
    public void testLifecycle_onCreate_onShow_onHide_onDestroy() {
        mBasicStream.onShow();
        mBasicStream.onHide();
        mBasicStream.onDestroy();
        verify(mAdapter).onDestroy();
        verify(mInitialModelProvider, never()).invalidate();
        verify(mInitialModelProvider).detachModelProvider();
        assertThat(mFakeFeedKnownContent.mListeners).isEmpty();
    }

    @Test
    public void testOnDestroy_destroysStreamDriver() {
        mBasicStream.onShow();
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(mStreamDriver);

        mBasicStream.onDestroy();

        verify(mStreamDriver).onDestroy();
    }

    @Test
    public void testOnDestroy_unregistersOnLayoutChangeListener() {
        // initial layout from 0, 0 will not rebind.
        getStreamRecyclerView().layout(0, 0, 100, 300);
        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 300, 100);
        verify(mAdapter).rebind();

        reset(mAdapter);
        mBasicStream.onDestroy();

        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 100, 300);
        verify(mAdapter, never()).rebind();
    }

    @Test
    public void testOnLayoutChange_signalsViewActionManager() {
        getStreamRecyclerView().layout(0, 0, 100, 300);
        verify(mActionManager, times(1)).onLayoutChange(); // Initial layout.

        getStreamRecyclerView().layout(0, 0, 300, 100); // New layout.
        verify(mActionManager, times(2)).onLayoutChange();
    }

    @Test
    public void testOnDestroy_deregistersSessionListener() {
        mBasicStream.onShow();

        mBasicStream.onDestroy();

        // Once for BasicStream, once for the session listener.
        verify(mInitialModelProvider, times(2)).unregisterObserver(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)
    public void testOnDestroy_setReachedUploadConditionsBitInActionManager_whenFeatureEnabled() {
        mBasicStream.onDestroy();

        // Verify that the upload bit is updated one time on #setupRecyclerView and one time
        // on #onDestroy.
        verify(mActionManager, times(2)).setCanUploadClicksAndViewsWhenNoticeCardIsPresent(true);
    }

    @Test
    public void testGetSavedInstanceStateString_beforeShow() throws InvalidProtocolBufferException {
        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(mBasicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasSessionId()).isFalse();
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_afterOnShow_beforeSessionStart()
            throws InvalidProtocolBufferException {
        when(mInitialModelProvider.getSessionId()).thenReturn(null);

        mBasicStream.onShow();

        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(mBasicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasSessionId()).isFalse();
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_afterOnShow_afterSessionStart()
            throws InvalidProtocolBufferException {
        mBasicStream.onShow();

        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(mBasicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.getSessionId()).isEqualTo(SESSION_ID);
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_noScrollRestoreBundle()
            throws InvalidProtocolBufferException {
        mBasicStream.onShow();

        when(mScrollRestorer.getScrollStateForScrollRestore(ADAPTER_HEADER_COUNT)).thenReturn(null);
        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(mBasicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasScrollState()).isFalse();
    }

    @Test
    public void testRestore() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);

        mBasicStream.onShow();

        verify(mRestoredModelProvider).registerObserver(mBasicStream);
    }

    @Test
    public void testRestore_withStringSavedState() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);

        mBasicStream.onShow();

        verify(mRestoredModelProvider).registerObserver(mBasicStream);
    }

    @Test
    public void testRestore_doesNotShowZeroState() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        reset(mStreamDriver);
        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);

        mBasicStream.onShow();

        verify(mStreamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(mStreamDriver, never()).showSpinner();
    }

    @Test
    public void testRestore_showsZeroStateIfNoSessionToRestore() {
        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate("");

        mBasicStream.onShow();

        verify(mStreamDriver, never()).showSpinner();

        mClock.advance(SPINNER_DELAY_MS);

        verify(mStreamDriver).showSpinner();
    }

    @Test
    public void testRestore_invalidSession() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);
        mBasicStream.onShow();

        verify(mModelProvider).registerObserver(mBasicStream);
    }

    @Test
    public void testRestore_invalidBase64Encoding() {
        mBasicStream.onShow();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        assertThatRunnable(() -> mBasicStream.onCreate("=invalid"))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testRestore_invalidProtocolBuffer() {
        mBasicStream.onShow();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        assertThatRunnable(()
                                   -> mBasicStream.onCreate(Base64.encodeToString(
                                           "invalid".getBytes(UTF_8), Base64.DEFAULT)))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testRestore_createsStreamDriver() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);

        mBasicStream.onShow();

        assertThat(mBasicStream.mStreamDriverRestoring).isTrue();
    }

    @Test
    public void testRestore_createsStreamDriver_afterFailure() {
        mBasicStream.onShow();

        String savedInstanceState = mBasicStream.getSavedInstanceStateString();

        mBasicStream.onHide();
        mBasicStream.onDestroy();

        when(mModelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(mRestoredModelProvider);

        mBasicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(mContext));
        mBasicStream.onCreate(savedInstanceState);

        // onSessionFinish indicates the restore has failed.
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        mBasicStream.onShow();

        assertThat(mBasicStream.mStreamDriverRestoring).isFalse();
    }

    @Test
    public void testTrim() {
        ShadowRecycledViewPool viewPool =
                Shadow.extract(getStreamRecyclerView().getRecycledViewPool());

        // RecyclerView ends up clearing the pool initially when the adapter is set on the
        // RecyclerView. Verify that has happened before anything else
        assertThat(viewPool.getClearCallCount()).isEqualTo(1);

        mBasicStream.trim();

        verify(mPietManager).purgeRecyclerPools();

        // We expect the clear() call to be 2 as RecyclerView ends up clearing the pool initially
        // when the adapter is set on the RecyclerView.  So one call for that and one call for
        // trim() call on stream.
        assertThat(viewPool.getClearCallCount()).isEqualTo(2);
    }

    @Test
    public void testLayoutChanges() {
        // initial layout from 0, 0 will not rebind.
        getStreamRecyclerView().layout(0, 0, 100, 300);
        verify(mAdapter, never()).rebind();
        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 300, 100);
        verify(mAdapter).rebind();
    }

    @Test
    public void testAddScrollListener() {
        ScrollListener scrollListener = mock(ScrollListener.class);
        mBasicStream.addScrollListener(scrollListener);
        verify(mScrollListenerNotifier).addScrollListener(scrollListener);
    }

    @Test
    public void testRemoveScrollListener() {
        ScrollListener scrollListener = mock(ScrollListener.class);
        mBasicStream.removeScrollListener(scrollListener);
        verify(mScrollListenerNotifier).removeScrollListener(scrollListener);
    }

    @Test
    public void testIsChildAtPositionVisible() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mBasicStream.isChildAtPositionVisible(-2)).isFalse();
        assertThat(mBasicStream.isChildAtPositionVisible(-1)).isFalse();
        assertThat(mBasicStream.isChildAtPositionVisible(0)).isTrue();
        assertThat(mBasicStream.isChildAtPositionVisible(1)).isTrue();
        assertThat(mBasicStream.isChildAtPositionVisible(2)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_nothingVisible() {
        assertThat(mBasicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validTop() {
        mLayoutManager.mFirstVisiblePosition = 0;
        assertThat(mBasicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validBottom() {
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mBasicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testGetChildTopAt_noVisibleChild() {
        assertThat(mBasicStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt_noChild() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mBasicStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        View view = new FrameLayout(mContext);
        mLayoutManager.addChildToPosition(0, view);

        assertThat(mBasicStream.getChildTopAt(0)).isEqualTo(view.getTop());
    }

    @Test
    public void testStreamContentVisible() {
        mBasicStream.setStreamContentVisibility(false);
        verify(mAdapter).setStreamContentVisible(false);

        reset(mAdapter);
        mBasicStream.setStreamContentVisibility(true);
        verify(mAdapter).setStreamContentVisible(true);
    }

    @Test
    public void testStreamContentVisible_notifiesItemAnimator_notVisible() {
        mBasicStream.setStreamContentVisibility(false);

        assertThat(((StreamItemAnimator) getStreamRecyclerView().getItemAnimator())
                           .getStreamContentVisibility())
                .isFalse();
    }

    @Test
    public void testStreamContentVisible_notifiesItemAnimator_visible() {
        mBasicStream.setStreamContentVisibility(true);

        assertThat(((StreamItemAnimator) getStreamRecyclerView().getItemAnimator())
                           .getStreamContentVisibility())
                .isTrue();
    }

    @Test
    public void testSetStreamContentVisibility_createsModelProvider_ifContentNotVisible() {
        mBasicStream.setStreamContentVisibility(false);
        mBasicStream.onShow();

        verifyNoMoreInteractions(mModelProvider);

        mBasicStream.setStreamContentVisibility(true);

        verify(mModelProviderFactory).createNew(any(ViewDepthProvider.class), any(UiContext.class));
    }

    @Test
    public void testSetStreamContentVisibility_resetsViewLogging() {
        mBasicStream.setStreamContentVisibility(false);
        mBasicStream.onShow();

        mBasicStream.setStreamContentVisibility(true);

        verify(mViewLoggingUpdater).resetViewTracking();
    }

    @Test
    public void testSetStreamContentVisibility_trueMultipleTimes_doesNotResetViewLogging() {
        mBasicStream.setStreamContentVisibility(true);
        mBasicStream.onShow();

        mBasicStream.setStreamContentVisibility(true);

        verify(mViewLoggingUpdater, never()).resetViewTracking();
    }

    @Test
    public void testTriggerRefresh() {
        mBasicStream.onShow();

        reset(mAdapter, mStreamDriver);

        mBasicStream.triggerRefresh();

        verify(mInitialModelProvider).triggerRefresh(RequestReason.HOST_REQUESTED);
        verify(mStreamDriver).showSpinner();
    }

    @Test
    public void testTriggerRefresh_beforeOnShow() {
        mBasicStream.triggerRefresh();

        verify(mInitialModelProvider, never()).triggerRefresh(anyInt());
        verify(mStreamDriver, never()).showSpinner();
    }

    @Test
    public void testAddOnContentChangedListener() {
        ContentChangedListener contentChangedListener = mock(ContentChangedListener.class);

        mBasicStream.addOnContentChangedListener(contentChangedListener);
        mBasicStream.mContentChangedListener.onContentChanged();

        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testRemoveOnContentChangedListener() {
        ContentChangedListener contentChangedListener = mock(ContentChangedListener.class);

        mBasicStream.addOnContentChangedListener(contentChangedListener);
        mBasicStream.removeOnContentChangedListener(contentChangedListener);

        mBasicStream.mContentChangedListener.onContentChanged();

        verify(contentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testOnShow_nonRestoringRestorer() {
        mBasicStream.onShow();

        assertThat(mBasicStream.mStreamDriverScrollRestorer).isEqualTo(mNonRestoringScrollRestorer);
    }

    @Test
    public void testOnShow_restoringScrollRestorer() {
        when(mInitialModelProvider.getCurrentState()).thenReturn(State.READY);
        mBasicStream.onShow();

        assertThat(mBasicStream.mStreamDriverScrollRestorer).isEqualTo(mScrollRestorer);
    }

    @Test
    public void testOnShow_setShown() {
        mBasicStream.onShow();

        verify(mAdapter).setShown(true);
    }

    @Test
    public void testOnShow_doesNotCreateModelProvider_ifStreamContentNotVisible() {
        mBasicStream.setStreamContentVisibility(false);

        mBasicStream.onShow();

        verifyNoMoreInteractions(mModelProviderFactory);
        verify(mAdapter, never()).setDriver(any(StreamDriver.class));
    }

    @Test
    public void testOnShow_restoresScrollPosition_ifStreamContentNotVisible() {
        mBasicStream.setStreamContentVisibility(false);

        mBasicStream.onShow();

        verify(mScrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testOnSessionStart_showsContentImmediately_ifNotInitialLoad() {
        mBasicStream.onShow();
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(mAdapter);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        verify(mAdapter).setDriver(mStreamDriver);
    }

    @Test
    public void testOnSessionStart_showsContentImmediately_ifSpinnerTimeElapsed() {
        mBasicStream.onShow();
        reset(mAdapter);

        // Advance so the spinner is shown.
        mClock.advance(SPINNER_DELAY_MS);

        // Advance so that it has shown long enough
        mClock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        // The adapter should be immediately set in onSesssionStart
        verify(mAdapter).setDriver(mStreamDriver);
    }

    @Test
    public void testOnSessionStart_showsContentWithDelay_ifSpinnerTimeNotElapsed() {
        mBasicStream.onShow();

        // Advance so the spinner is shown.
        mClock.advance(SPINNER_DELAY_MS);

        reset(mAdapter);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        verify(mAdapter, never()).setDriver(any(StreamDriver.class));

        // Advance so that it has shown long enough.
        mClock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        verify(mAdapter).setDriver(mStreamDriver);
    }

    @Test
    public void testOnSessionStart_doesNotShowContent_ifSessionFinishesBeforeSpinnerTimeElapsed() {
        mClock.set(10);

        mBasicStream.onShow();

        reset(mAdapter);

        // Advance so the spinner is shown.
        mClock.advance(SPINNER_DELAY_MS);

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        // Advance so that it has shown long enough.
        mClock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        verify(mAdapter, never()).setDriver(mStreamDriver);
    }

    @Test
    public void testOnSessionStart_showsZeroState_ifFeatureDriversEmpty() {
        mBasicStream.onShow();

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(mStreamDriver).showZeroState(ZeroStateShowReason.NO_CONTENT);
    }

    @Test
    public void testOnShow_delaysShowingZeroState_onInitialLoad() {
        mBasicStream.onShow();

        verify(mStreamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(mStreamDriver, never()).showSpinner();

        // Advance so the spinner is shown.
        mClock.advance(SPINNER_DELAY_MS);

        verify(mStreamDriver).showSpinner();
    }

    @Test
    public void testOnShow_delaysShowingZeroState_configured_delay_time() {
        mBasicStream.onShow();

        verify(mStreamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(mStreamDriver, never()).showSpinner();

        // Advance so the spinner is shown.
        mClock.advance(SPINNER_DELAY_MS);

        verify(mStreamDriver).showSpinner();
    }

    @Test
    public void testOnHide_setShown() {
        mBasicStream.onShow();
        reset(mAdapter);

        mBasicStream.onHide();

        verify(mAdapter).setShown(false);
    }

    @Test
    public void testOnShow_signalsViewActionManager() {
        mBasicStream.onShow();
        verify(mActionManager).onShow();
    }

    @Test
    public void testOnHide_signalsViewActionManager() {
        mBasicStream.onHide();
        verify(mActionManager).onHide();
    }

    @Test
    public void testOnHide_dismissesPopup() {
        mBasicStream.onHide();
        verify(mContextMenuManager).dismissPopup();
    }

    @Test
    public void onSessionFinished_afterOnDestroy_unregistersOnce() {
        mBasicStream.onShow();

        mBasicStream.onDestroy();
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        // Should only occur once between onDestroy() and onSessionFinished()
        verify(mInitialModelProvider).unregisterObserver(mBasicStream);
    }

    @Test
    public void onDestroyTwice_unregistersOnce() {
        mBasicStream.onShow();

        mBasicStream.onDestroy();
        mBasicStream.onDestroy();

        // Should only occur once between multiple onDestroy() calls
        verify(mInitialModelProvider).unregisterObserver(mBasicStream);
    }

    @Test
    public void onSessionStart_propagatesUiContext() {
        mBasicStream.onShow();

        UiRefreshReason uiRefreshReason =
                UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build();
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        mBasicStream.onSessionFinished(
                UiContext.newBuilder()
                        .setExtension(UiRefreshReason.uiRefreshReasonExtension, uiRefreshReason)
                        .build());
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        assertThat(mBasicStream.mStreamDriverUiRefreshReason).isEqualTo(uiRefreshReason);
    }

    @Test
    public void onShow_whileStreamContentNotVisible_logsOpenedWithNoContent() {
        mBasicStream.setStreamContentVisibility(false);

        mBasicStream.onShow();

        verify(mBasicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void onShow_whileStreamContentNotVisible_twice_logsOpenedWithNoContentOnce() {
        mBasicStream.setStreamContentVisibility(false);

        mBasicStream.onShow();
        mBasicStream.onHide();
        mBasicStream.onShow();

        verify(mBasicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void failToRestoreSession_doesntShowSpinnerImmediately() {
        mBasicStream.onShow();

        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(mStreamDriver, never()).showSpinner();
    }

    @Test
    public void failToRestoreSession_showsSpinnerAfterDelay() {
        mBasicStream.onShow();

        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        mClock.advance(SPINNER_DELAY_MS);

        verify(mStreamDriver).showSpinner();
    }

    @Test
    public void startSession_quicklyFinish_doesntShowInitialSpinner() {
        mBasicStream.onShow();

        // Advance, but not enough so that the spinner shows.
        mClock.advance(SPINNER_DELAY_MS / 2);

        // Start the session. At this point, the spinner enqueued by onShow() should never be shown.
        mBasicStream.onSessionStart(UiContext.getDefaultInstance());

        // Finish the session immediately. As we had started a session and the user had presumably
        // seen content, we shouldn't trigger the spinner enqueued by onShow().
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        // It has now been long enough since onShow that the spinner would show if onSessionStart()
        // hadn't been called first.
        mClock.advance(SPINNER_DELAY_MS / 2 + 1);

        verify(mStreamDriver, never()).showSpinner();
    }

    @Test
    public void startSession_thenFinish_showsSpinnerAfterDelay() {
        mBasicStream.onShow();

        mBasicStream.onSessionStart(UiContext.getDefaultInstance());
        mBasicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(mStreamDriver, never()).showSpinner();

        mClock.advance(SPINNER_DELAY_MS);

        verify(mStreamDriver).showSpinner();
    }

    private byte[] decodeSavedInstanceStateString(String savedInstanceState) {
        return Base64.decode(savedInstanceState, Base64.DEFAULT);
    }

    private RecyclerView getStreamRecyclerView() {
        return (RecyclerView) mBasicStream.getView();
    }

    private BasicStreamForTest createBasicStream(LinearLayoutManager layoutManager) {
        return new BasicStreamForTest(mContext, mStreamConfiguration, mock(CardConfiguration.class),
                mock(ImageLoaderApi.class), mock(ActionParserFactory.class), mock(ActionApi.class),
                mock(CustomElementProvider.class), DebugBehavior.VERBOSE, new ThreadUtils(),
                mHeaders, mClock, mModelProviderFactory, new HostBindingProvider(), mActionManager,
                CONFIGURATION, layoutManager, mock(OfflineIndicatorApi.class), mStreamDriver);
    }

    private class BasicStreamForTest extends BasicStream {
        private final LinearLayoutManager mLayoutManager;
        private StreamDriver mStreamDriver;
        private boolean mStreamDriverRestoring;

        private ScrollRestorer mStreamDriverScrollRestorer;
        private StreamContentChangedListener mContentChangedListener;
        private UiRefreshReason mStreamDriverUiRefreshReason;

        public BasicStreamForTest(Context context, StreamConfiguration streamConfiguration,
                CardConfiguration cardConfiguration, ImageLoaderApi imageLoaderApi,
                ActionParserFactory actionParserFactory, ActionApi actionApi,
                /*@Nullable*/ CustomElementProvider customElementProvider,
                DebugBehavior debugBehavior, ThreadUtils threadUtils, List<Header> headers,
                Clock clock, ModelProviderFactory modelProviderFactory,
                /*@Nullable*/ HostBindingProvider hostBindingProvider, ActionManager actionManager,
                Configuration configuration, LinearLayoutManager layoutManager,
                OfflineIndicatorApi offlineIndicatorApi, StreamDriver streamDriver) {
            super(context, streamConfiguration, cardConfiguration, imageLoaderApi,
                    actionParserFactory, actionApi, customElementProvider, debugBehavior,
                    threadUtils, headers, clock, modelProviderFactory, hostBindingProvider,
                    actionManager, configuration, mSnackbarApi, mBasicLoggingApi,
                    offlineIndicatorApi,

                    mMainThreadRunner, mFakeFeedKnownContent, mTooltipApi,
                    /* isBackgroundDark= */ false, /* isPlaceholderShown= */ false);
            this.mLayoutManager = layoutManager;
            this.mStreamDriver = streamDriver;
        }

        @Override
        PietManager createPietManager(Context context, CardConfiguration cardConfiguration,
                ImageLoaderApi imageLoaderApi,
                /*@Nullable*/ CustomElementProvider customElementProvider,
                DebugBehavior debugBehavior, Clock clock,
                /*@Nullable*/ HostBindingProvider hostBindingProvider,
                StreamOfflineMonitor streamOfflineMonitor, Configuration config,
                boolean isBackgroundDark) {
            return mPietManager;
        }

        @Override
        StreamDriver createStreamDriver(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, ModelProvider modelProvider,
                ThreadUtils threadUtils, Clock clock, Configuration configuration, Context context,
                SnackbarApi snackbarApi, ContentChangedListener contentChangedListener,
                ScrollRestorer scrollRestorer, BasicLoggingApi basicLoggingApi,
                StreamOfflineMonitor streamOfflineMonitor, FeedKnownContent feedKnownContent,
                ContextMenuManager contextMenuManager, boolean restoring, boolean isInitialLoad,
                MainThreadRunner mainThreadRunner, TooltipApi tooltipApi,
                UiRefreshReason uiRefreshReason, ScrollListenerNotifier scrollListenerNotifier) {
            mStreamDriverScrollRestorer = scrollRestorer;
            mStreamDriverRestoring = restoring;
            this.mStreamDriverUiRefreshReason = uiRefreshReason;
            return mStreamDriver;
        }

        @Override
        StreamContentChangedListener createStreamContentChangedListener() {
            mContentChangedListener = new StreamContentChangedListener();
            return mContentChangedListener;
        }

        @Override
        StreamRecyclerViewAdapter createRecyclerViewAdapter(Context context,
                CardConfiguration cardConfiguration, PietManager pietManager,
                DeepestContentTracker deepestContentTracker,
                StreamContentChangedListener streamContentChangedListener,
                ScrollObservable scrollObservable, Configuration configuration,
                PietEventLogger pietEventLogger) {
            return mAdapter;
        }

        @Override
        ScrollListenerNotifier createScrollListenerNotifier(
                ContentChangedListener contentChangedListener,
                BasicStreamScrollMonitor scrollMonitor, MainThreadRunner mainThreadRunner) {
            return mScrollListenerNotifier;
        }

        @Override
        LinearLayoutManager createRecyclerViewLayoutManager(Context context) {
            return mLayoutManager;
        }

        @Override
        ScrollRestorer createScrollRestorer(Configuration configuration, RecyclerView recyclerView,
                ScrollListenerNotifier scrollListenerNotifier,
                /*@Nullable*/ ScrollState scrollState) {
            return mScrollRestorer;
        }

        @Override
        ScrollRestorer createNonRestoringScrollRestorer(Configuration configuration,
                RecyclerView recyclerView, ScrollListenerNotifier scrollListenerNotifier) {
            return mNonRestoringScrollRestorer;
        }

        @Override
        ContextMenuManagerImpl createContextMenuManager(
                RecyclerView recyclerView, MenuMeasurer menuMeasurer) {
            return mContextMenuManager;
        }

        @Override
        ViewLoggingUpdater createViewLoggingUpdater() {
            return mViewLoggingUpdater;
        }
    }

    private class LinearLayoutManagerWithFakePositioning extends LinearLayoutManager {
        private final List<View> mChildMap;
        private int mFirstVisiblePosition = RecyclerView.NO_POSITION;
        private int mLastVisiblePosition = RecyclerView.NO_POSITION;

        public LinearLayoutManagerWithFakePositioning(Context context) {
            super(context);
            mChildMap = new ArrayList<>();
        }

        @Override
        public int findFirstVisibleItemPosition() {
            return mFirstVisiblePosition;
        }

        @Override
        public int findLastVisibleItemPosition() {
            return mLastVisiblePosition;
        }

        @Override
        public View findViewByPosition(int i) {
            if (i < 0 || i >= mChildMap.size()) {
                return null;
            }
            return mChildMap.get(i);
        }

        private void addChildToPosition(int position, View child) {
            mChildMap.add(position, child);
        }
    }

    private static class FakeFeedKnownContent implements FeedKnownContent {
        private final Set<KnownContent.Listener> mListeners = new HashSet<>();

        @Override
        public void getKnownContent(Consumer<List<ContentMetadata>> knownContentConsumer) {}

        @Override
        public void addListener(KnownContent.Listener listener) {
            mListeners.add(listener);
        }

        @Override
        public void removeListener(KnownContent.Listener listener) {
            mListeners.remove(listener);
        }

        @Override
        public Listener getKnownContentHostNotifier() {
            return null;
        }
    }
}
