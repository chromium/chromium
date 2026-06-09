// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.annotation.Px;
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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
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
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link FeedSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
public class FeedSurfaceMediatorTest {
    static final @Px int TOOLBAR_HEIGHT = 10;
    private static final int SPAN_COUNT_SMALL_WIDTH = 1;
    private static final int SPAN_COUNT_LARGE_WIDTH = 2;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Mocked JNI.
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    @Mock private RecyclerView mRecyclerView;
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
    @Captor private ArgumentCaptor<View.OnLayoutChangeListener> mLayoutChangeListenerCaptor;

    private final Context mContext = RuntimeEnvironment.application;
    private Activity mActivity;
    private FeedSurfaceMediator mFeedSurfaceMediator;

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        // Print logs to stdout.

        mActivity = Robolectric.buildActivity(Activity.class).get();
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);

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
        testUpdateLayoutImpl(600, SPAN_COUNT_SMALL_WIDTH);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testUpdateLayout_largeWidth_tablet() {
        testUpdateLayoutImpl(800, SPAN_COUNT_LARGE_WIDTH);
    }

    private void testUpdateLayoutImpl(int width, int expectedSpanCount) {
        DeviceFormFactor.setIsTabletForTesting(true);
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

        verify(mRecyclerView).addOnLayoutChangeListener(mLayoutChangeListenerCaptor.capture());
        clearInvocations(mListLayoutHelper);

        mLayoutChangeListenerCaptor.getValue().onLayoutChange(null, 0, 0, width, 1000, 0, 0, 0, 0);

        verify(mListLayoutHelper).setColumnCount(expectedSpanCount);
    }

    private FeedSurfaceMediator createMediator() {
        return new FeedSurfaceMediator(
                mFeedSurfaceCoordinator, mActivity, null, /* actionDelegate= */ null, mProfileMock);
    }
}
