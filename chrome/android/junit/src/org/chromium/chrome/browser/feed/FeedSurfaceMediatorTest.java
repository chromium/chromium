// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider.RestoringState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtilsJni;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link FeedSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedSurfaceMediatorTest {
    static final @Px int TOOLBAR_HEIGHT = 10;
    private static final int SPAN_COUNT_SMALL_WIDTH = 1;
    private static final int SPAN_COUNT_LARGE_WIDTH = 2;
    private static final int SPAN_COUNT_RESPONSIVE_LAYOUT = 1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Mocked JNI.
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private GestureNavigationUtils.Natives mGestureNavigationUtilsJniMock;
    @Mock private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfileMock;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TemplateUrlService mUrlService;
    @Mock private FeedStream mForYouStream;
    @Mock private HybridListRenderer mHybridListRenderer;
    @Mock private ListLayoutHelper mListLayoutHelper;
    @Mock private FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    @Mock private FeedReliabilityLogger mReliabilityLogger;
    @Captor private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserverCaptor;

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FeedSurfaceMediator mFeedSurfaceMediator;

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        // Print logs to stdout.

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mRecyclerView = new RecyclerView(mActivity);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        GestureNavigationUtilsJni.setInstanceForTesting(mGestureNavigationUtilsJniMock);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // We want to make the feed service bridge ignore the ablation flag.
        when(mFeedServiceBridgeJniMock.isEnabled())
                .thenAnswer(invocation -> mPrefService.getBoolean(Pref.ENABLE_SNIPPETS));
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount()).thenReturn(true);
        when(mFeedSurfaceCoordinator.isActive()).thenReturn(true);
        when(mFeedSurfaceCoordinator.getRecyclerView()).thenReturn(mRecyclerView);
        when(mFeedSurfaceCoordinator.createFeedStream(
                        eq(StreamKind.FOR_YOU), any(Stream.StreamsMediator.class)))
                .thenReturn(mForYouStream);
        when(mFeedSurfaceCoordinator.getReliabilityLogger()).thenReturn(mReliabilityLogger);
        when(mFeedSurfaceCoordinator.getHybridListRenderer()).thenReturn(mHybridListRenderer);
        when(mHybridListRenderer.getListLayoutHelper()).thenReturn(mListLayoutHelper);
        when(mListLayoutHelper.setColumnCount(anyInt())).thenReturn(true);
        when(mFeedSurfaceCoordinator.getSurfaceLifecycleManager())
                .thenReturn(mFeedSurfaceLifecycleManager);
        when(mFeedSurfaceCoordinator.getView()).thenReturn(mRecyclerView);
        SettableNonNullObservableSupplier<Boolean> hasUnreadContent =
                ObservableSuppliers.createNonNull(false);
        when(mForYouStream.hasUnreadContent()).thenReturn(hasUnreadContent);
        when(mForYouStream.getStreamKind()).thenReturn(StreamKind.FOR_YOU);

        FeedSurfaceMediator.setPrefForTest(mPrefChangeRegistrar, mPrefService);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);
    }

    @After
    public void tearDown() {
        if (mFeedSurfaceMediator != null) mFeedSurfaceMediator.destroy();
        FeedSurfaceMediator.setPrefForTest(null, null);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY);
    }

    @Test
    public void testSerializeScrollState() {
        FeedScrollState state = new FeedScrollState();
        state.position = 2;
        state.lastPosition = 4;
        state.offset = 50;
        state.feedContentState = "foo";

        FeedScrollState deserializedState = FeedScrollState.fromJson(state.toJson());

        assertEquals(2, deserializedState.position);
        assertEquals(4, deserializedState.lastPosition);
        assertEquals(50, deserializedState.offset);
        assertEquals("foo", deserializedState.feedContentState);
        assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testSerializeScrollStateAllFieldsUnset() {
        FeedScrollState state = new FeedScrollState();

        FeedScrollState deserializedState = FeedScrollState.fromJson(state.toJson());

        assertEquals(state.position, deserializedState.position);
        assertEquals(state.lastPosition, deserializedState.lastPosition);
        assertEquals(state.offset, deserializedState.offset);
        assertEquals(state.feedContentState, deserializedState.feedContentState);
        assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testScrollStateFromInvalidJson() {
        assertEquals(null, FeedScrollState.fromJson("{{=xcg"));
    }

    @Test
    public void updateContent_openingTabIdForYou() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.updateContent();

        verify(mForYouStream, times(1)).bind(any(), any(), any(), any(), any(), any(), anyInt());
        assertTrue(mFeedSurfaceMediator.hasStreams());
    }

    @Test
    public void testUpdateContent_policyFeedOnOff() {
        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();
        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertFalse(mFeedSurfaceMediator.hasStreams());
    }

    @Test
    public void testUpdateContent_policyFeedOffOn() {
        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(true);
        assertTrue(mFeedSurfaceMediator.hasStreams());
    }

    @Test
    public void testObserveTemplateUrlService() {
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        doReturn(true).when(mUrlService).isDefaultSearchEngineGoogle();

        mFeedSurfaceMediator = createMediator();
        // Verifies that an observer is added to the TemplateUrlService.
        verify(mUrlService).addObserver(mTemplateUrlServiceObserverCaptor.capture());
        verify(mPrefService).setBoolean(eq(Pref.ENABLE_SNIPPETS_BY_DSE), eq(true));

        // Verifies Pref.ENABLE_SNIPPETS_BY_DSE is updated when
        // TemplateUrlService#onTemplateURLServiceChanged() is called.
        doReturn(false).when(mUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        verify(mPrefService).setBoolean(eq(Pref.ENABLE_SNIPPETS_BY_DSE), eq(false));

        // Verifies that observer isn't removed when the Feeds become invisible.
        mFeedSurfaceMediator.destroyPropertiesForStream();
        verify(mUrlService, never()).removeObserver(mTemplateUrlServiceObserverCaptor.capture());
    }

    @Test
    public void testOnSurfaceClosed() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();

        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mForYouStream).unbind(anyBoolean(), anyBoolean());
    }

    @Test
    public void testshowOrHideFeed_GseOnAndThenOffAndThenOn() {
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.updateContent();
        mFeedSurfaceMediator.showOrHideFeed();

        assertNotNull(mFeedSurfaceMediator.getCurrentStreamForTesting());

        // Turn GSE off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        mFeedSurfaceMediator.updateContent();
        mFeedSurfaceMediator.showOrHideFeed();

        assertNull(mFeedSurfaceMediator.getCurrentStreamForTesting());

        // Turn GSE on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        mFeedSurfaceMediator.updateContent();
        mFeedSurfaceMediator.showOrHideFeed();

        assertNotNull(mFeedSurfaceMediator.getCurrentStreamForTesting());
    }

    @Test
    public void testshowOrHideFeed_afterDestroy() {
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.showOrHideFeed();

        // Calling showOrHideFeed after destroy should not cause any crash.
        mFeedSurfaceMediator.destroy();
        mFeedSurfaceMediator.showOrHideFeed();
    }

    @Test
    public void testStreamsMediatorImpl_refreshStream() {
        mFeedSurfaceMediator = createMediator();
        Stream.StreamsMediator streamsMediator = mFeedSurfaceMediator.new StreamsMediatorImpl();

        streamsMediator.refreshStream();

        verify(mFeedSurfaceCoordinator).nonSwipeRefresh();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testUpdateLayout_smallWidth_tablet() {
        DeviceFormFactor.setIsTabletForTesting(true);
        testUpdateLayoutImpl(600, SPAN_COUNT_SMALL_WIDTH);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testUpdateLayout_largeWidth_tablet() {
        DeviceFormFactor.setIsTabletForTesting(true);
        testUpdateLayoutImpl(800, SPAN_COUNT_LARGE_WIDTH);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.WIDE_SCREEN_FEED_FOR_FOLDABLES)
    public void testUpdateLayout_responsive_foldable() {
        DeviceInfo.setIsFoldableForTesting(true);
        testUpdateLayoutImpl(800, SPAN_COUNT_RESPONSIVE_LAYOUT);
    }

    private void testUpdateLayoutImpl(int width, int expectedSpanCount) {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator =
                new FeedSurfaceMediator(
                        mFeedSurfaceCoordinator,
                        mActivity,
                        mock(SnapScrollHelper.class),
                        /* actionDelegate= */ null,
                        mProfileMock);
        mFeedSurfaceMediator.updateContent();

        clearInvocations(mListLayoutHelper);

        mRecyclerView.layout(0, 0, width, 1000);

        verify(mListLayoutHelper).setColumnCount(expectedSpanCount);
    }

    @Test
    public void testScrollListenerRegisteredOnCreation() {
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        // Verify scroll listener is added to the RecyclerView during creation.
        assertRecyclerViewScrollForwardedOnce();
    }

    @Test
    public void testScrollListenerNotToggledWithFeed() {
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        // 1. Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        // 2. Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        // 3. Turn feed back on so that scroll events are forwarded.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        // Verify scroll listener is not added again or removed.
        assertRecyclerViewScrollForwardedOnce();
    }

    /** Scrolls the RecyclerView and verifies the scroll state change is forwarded exactly once. */
    private void assertRecyclerViewScrollForwardedOnce() {
        ScrollListener listener = mock(ScrollListener.class);
        mFeedSurfaceMediator.addScrollListener(listener);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));

        mRecyclerView.smoothScrollBy(0, 100);

        verify(listener, times(1)).onScrollStateChanged(RecyclerView.SCROLL_STATE_SETTLING);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FEED_NULL_ITEM_ANIMATOR_ON_SCROLL_RESTORE)
    public void testScrollRestore_disablesItemAnimatorUntilRestoreCompletes() {
        when(mGestureNavigationUtilsJniMock.shouldAnimateBackForwardTransitions()).thenReturn(true);
        RecyclerView.ItemAnimator originalAnimator = new DefaultItemAnimator();
        TestRecyclerView recyclerView = createTestRecyclerView(originalAnimator);
        createMediatorWithScrollStateToRestore(/* position= */ 3);

        // The list reaches the saved position.
        when(mListLayoutHelper.findFirstVisibleItemPosition()).thenReturn(3);
        recyclerView.scrollAndRunScrollCallback();

        // Item animations are disabled with a null animator, not a custom no-op one.
        assertNull(recyclerView.getItemAnimator());
        assertEquals(RestoringState.WAITING_TO_RESTORE, getRestoringState());

        // Once RecyclerView has finished animating, the original animator is restored.
        shadowOf(Looper.getMainLooper()).idle();

        assertEquals(originalAnimator, recyclerView.getItemAnimator());
        assertEquals(RestoringState.RESTORED, getRestoringState());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FEED_NULL_ITEM_ANIMATOR_ON_SCROLL_RESTORE)
    public void testScrollRestore_featureFlagDisabled_usesNoOpItemAnimator() {
        when(mGestureNavigationUtilsJniMock.shouldAnimateBackForwardTransitions()).thenReturn(true);
        RecyclerView.ItemAnimator originalAnimator = new DefaultItemAnimator();
        TestRecyclerView recyclerView = createTestRecyclerView(originalAnimator);
        createMediatorWithScrollStateToRestore(/* position= */ 3);

        when(mListLayoutHelper.findFirstVisibleItemPosition()).thenReturn(3);
        recyclerView.scrollAndRunScrollCallback();

        // With the feature flag off, the previous no-op animator is installed instead of null.
        assertNotNull(recyclerView.getItemAnimator());
        assertNotEquals(originalAnimator, recyclerView.getItemAnimator());

        // The original animator is still restored afterwards.
        shadowOf(Looper.getMainLooper()).idle();

        assertEquals(originalAnimator, recyclerView.getItemAnimator());
        assertEquals(RestoringState.RESTORED, getRestoringState());
    }

    @Test
    public void testScrollRestore_beforeReachingSavedPosition_keepsItemAnimator() {
        when(mGestureNavigationUtilsJniMock.shouldAnimateBackForwardTransitions()).thenReturn(true);
        RecyclerView.ItemAnimator originalAnimator = new DefaultItemAnimator();
        TestRecyclerView recyclerView = createTestRecyclerView(originalAnimator);
        createMediatorWithScrollStateToRestore(/* position= */ 3);

        when(mListLayoutHelper.findFirstVisibleItemPosition()).thenReturn(1);
        recyclerView.scrollAndRunScrollCallback();

        assertEquals(originalAnimator, recyclerView.getItemAnimator());
        shadowOf(Looper.getMainLooper()).idle();
        assertEquals(RestoringState.WAITING_TO_RESTORE, getRestoringState());
    }

    @Test
    public void testScrollRestore_withoutBackForwardTransitions_keepsItemAnimator() {
        when(mGestureNavigationUtilsJniMock.shouldAnimateBackForwardTransitions())
                .thenReturn(false);
        RecyclerView.ItemAnimator originalAnimator = new DefaultItemAnimator();
        TestRecyclerView recyclerView = createTestRecyclerView(originalAnimator);
        createMediatorWithScrollStateToRestore(/* position= */ 3);

        when(mListLayoutHelper.findFirstVisibleItemPosition()).thenReturn(3);
        recyclerView.scrollAndRunScrollCallback();

        assertEquals(originalAnimator, recyclerView.getItemAnimator());
    }

    /**
     * Uses a real RecyclerView to check that list changes laid out while the scroll restore has
     * item animations disabled don't leave stale views behind, e.g. a stuck loading spinner.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FEED_NULL_ITEM_ANIMATOR_ON_SCROLL_RESTORE)
    public void testScrollRestore_listChangesDuringRestore_leaveNoStaleViews() {
        when(mGestureNavigationUtilsJniMock.shouldAnimateBackForwardTransitions()).thenReturn(true);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout root = new FrameLayout(activity);
        activity.setContentView(root);
        RecyclerView recyclerView = new RecyclerView(activity);
        recyclerView.setLayoutManager(new LinearLayoutManager(activity));
        TestAdapter adapter = new TestAdapter();
        recyclerView.setAdapter(adapter);
        RecyclerView.ItemAnimator originalAnimator = new DefaultItemAnimator();
        recyclerView.setItemAnimator(originalAnimator);
        root.addView(
                recyclerView, new FrameLayout.LayoutParams(400, 5 * TestAdapter.ITEM_HEIGHT_PX));
        for (int i = 0; i < 10; i++) adapter.add(adapter.getItemCount(), "card" + i);
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(3));

        when(mFeedSurfaceCoordinator.getRecyclerView()).thenReturn(recyclerView);
        when(mFeedSurfaceCoordinator.getView()).thenReturn(recyclerView);
        createMediatorWithScrollStateToRestore(/* position= */ 1);
        when(mListLayoutHelper.findFirstVisibleItemPosition()).thenReturn(1);

        // The restore scroll reaches the saved position. In the same frame, the feed removes an
        // on-screen card and shows a loading spinner.
        recyclerView.scrollBy(0, TestAdapter.ITEM_HEIGHT_PX);
        adapter.remove("card2");
        adapter.add(2, "spinner");
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(3));

        assertEquals(RestoringState.RESTORED, getRestoringState());
        assertEquals(originalAnimator, recyclerView.getItemAnimator());
        assertEquals(new ArrayList<String>(), getStaleChildTags(recyclerView));
        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View child = recyclerView.getChildAt(i);
            assertTrue(
                    "Item " + child.getTag() + " was never released",
                    recyclerView.getChildViewHolder(child).isRecyclable());
        }

        // Later, the spinner is removed while on screen, using the original animator.
        adapter.remove("spinner");
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(3));

        assertEquals(new ArrayList<String>(), getStaleChildTags(recyclerView));
    }

    /** Creates a mediator with a bound feed and a saved scroll position to restore. */
    private void createMediatorWithScrollStateToRestore(int position) {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.updateContent();

        FeedScrollState state = new FeedScrollState();
        state.position = position;
        mFeedSurfaceMediator.restoreSavedInstanceState(state.toJson());
    }

    /** Makes the mediator use a new {@link TestRecyclerView} with the given item animator. */
    private TestRecyclerView createTestRecyclerView(RecyclerView.ItemAnimator itemAnimator) {
        TestRecyclerView recyclerView = new TestRecyclerView(mActivity);
        recyclerView.setItemAnimator(itemAnimator);
        when(mFeedSurfaceCoordinator.getRecyclerView()).thenReturn(recyclerView);
        when(mFeedSurfaceCoordinator.getView()).thenReturn(recyclerView);
        return recyclerView;
    }

    private int getRestoringState() {
        return mFeedSurfaceMediator.getRestoringStateSupplier().get();
    }

    /** Returns tags of children that the RecyclerView's LayoutManager no longer tracks. */
    private static List<String> getStaleChildTags(RecyclerView recyclerView) {
        RecyclerView.LayoutManager layoutManager = recyclerView.getLayoutManager();
        List<View> tracked = new ArrayList<>();
        for (int i = 0; i < layoutManager.getChildCount(); i++) {
            tracked.add(layoutManager.getChildAt(i));
        }
        List<String> stale = new ArrayList<>();
        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View child = recyclerView.getChildAt(i);
            if (!tracked.contains(child)) stale.add(String.valueOf(child.getTag()));
        }
        return stale;
    }

    /**
     * A real RecyclerView, as View mocks are not allowed, that hands the test the scroll listener
     * added by the mediator and the callback it posts for the next frame. The view isn't attached
     * to a window, so it would never run that callback itself.
     */
    private static class TestRecyclerView extends RecyclerView {
        private OnScrollListener mScrollListener;
        private Runnable mAnimationCallback;

        TestRecyclerView(Context context) {
            super(context);
        }

        @Override
        public void addOnScrollListener(OnScrollListener listener) {
            super.addOnScrollListener(listener);
            mScrollListener = listener;
        }

        @Override
        public void postOnAnimation(Runnable action) {
            mAnimationCallback = action;
        }

        /** Notifies the mediator of a scroll and runs the callback it posts for the next frame. */
        void scrollAndRunScrollCallback() {
            mAnimationCallback = null;
            mScrollListener.onScrolled(this, 0, 0);
            assertNotNull(mAnimationCallback);
            mAnimationCallback.run();
        }
    }

    /** A minimal adapter whose item views are tagged with their key. */
    private static class TestAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        static final int ITEM_HEIGHT_PX = 100;
        private final List<String> mItems = new ArrayList<>();

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new FrameLayout(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT_PX));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
            holder.itemView.setTag(mItems.get(position));
        }

        @Override
        public int getItemCount() {
            return mItems.size();
        }

        void add(int position, String key) {
            mItems.add(position, key);
            notifyItemInserted(position);
        }

        void remove(String key) {
            int position = mItems.indexOf(key);
            mItems.remove(position);
            notifyItemRemoved(position);
        }
    }

    private FeedSurfaceMediator createMediator() {
        return new FeedSurfaceMediator(
                mFeedSurfaceCoordinator, mActivity, null, /* actionDelegate= */ null, mProfileMock);
    }
}
