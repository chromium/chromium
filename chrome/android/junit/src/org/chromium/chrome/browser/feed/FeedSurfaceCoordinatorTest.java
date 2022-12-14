// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.MotionEvent;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderView;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * Tests for {@link FeedSurfaceCoordinator}.
 *
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures({
        ChromeFeatureList.WEB_FEED,
        ChromeFeatureList.WEB_FEED_SORT,
        ChromeFeatureList.WEB_FEED_ONBOARDING,
        ChromeFeatureList.INTEREST_FEED_V2_AUTOPLAY,
        ChromeFeatureList.FEED_INTERACTIVE_REFRESH,
        ChromeFeatureList.FEED_BACK_TO_TOP,
        ChromeFeatureList.FEED_MULTI_COLUMN,
        // TODO(crbug.com/1353777): Disabling the feature explicitly, because native is not
        // available to provide a default value. This should be enabled if the feature is enabled by
        // default or removed if the flag is removed.
        ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS,
})
@Features.EnableFeatures(
        {ChromeFeatureList.FEED_CLIENT_GOOD_VISITS, ChromeFeatureList.FEED_HEADER_STICK_TO_TOP})
public class FeedSurfaceCoordinatorTest {
    private static final @SurfaceType int SURFACE_TYPE = SurfaceType.NEW_TAB_PAGE;
    private static final long SURFACE_CREATION_TIME_NS = 1234L;

    private class TestLifecycleManager extends FeedSurfaceLifecycleManager {
        public TestLifecycleManager(Activity activity, FeedSurfaceCoordinator coordinator) {
            super(activity, coordinator);
        }

        @Override
        public boolean canShow() {
            return true;
        }
    }

    private class TestSurfaceDelegate implements FeedSurfaceDelegate {
        @Override
        public FeedSurfaceLifecycleManager createStreamLifecycleManager(
                Activity activity, SurfaceCoordinator coordinator) {
            mLifecycleManager =
                    new TestLifecycleManager(activity, (FeedSurfaceCoordinator) coordinator);
            return mLifecycleManager;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent e) {
            return false;
        }
    }

    private class TestTabModel extends EmptyTabModel {
        public ArrayList<TabModelObserver> mObservers = new ArrayList<TabModelObserver>();

        @Override
        public void addObserver(TabModelObserver observer) {
            mObservers.add(observer);
        }

        void selectTab() {
            for (TabModelObserver observer : mObservers) {
                observer.didSelectTab(null, 0, 0);
            }
        }
    }
    private TestTabModel mTabModel = new TestTabModel();
    private TestTabModel mTabModelIncognito = new TestTabModel();

    private FeedSurfaceCoordinator mCoordinator;

    @Rule
    public JniMocker mocker = new JniMocker();

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    @Mock
    private LinearLayoutManager mLayoutManager;
    private TestLifecycleManager mLifecycleManager;

    // Mocked Direct dependencies.
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private SnapScrollHelper mSnapHelper;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock
    private SectionHeaderView mSectionHeaderView;
    @Mock
    private FeedActionDelegate mFeedActionDelegate;

    // Mocked JNI.
    @Mock
    private FeedStream.Natives mFeedStreamJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    private FeedProcessScopeDependencyProvider.Natives mProcessScopeJniMock;
    @Mock
    private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;

    // Mocked xSurface setup.
    @Mock
    private ProcessScope mProcessScope;
    @Mock
    private SurfaceScope mSurfaceScope;
    @Mock
    private HybridListRenderer mRenderer;
    @Captor
    private ArgumentCaptor<NtpListContentManager> mContentManagerCaptor;

    // Mocked indirect dependencies.
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private Profile mProfileMock;
    @Mock
    private IdentityServicesProvider mIdentityService;
    @Mock
    private SigninManager mSigninManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock
    private PrefService mPrefService;
    @Mock
    private TemplateUrlService mUrlService;
    @Mock
    private Resources mResources;
    @Mock
    private RecyclerView.Adapter mAdapter;
    @Mock
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock
    private Tracker mTracker;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private ScrollableContainerDelegate mScrollableContainerDelegate;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mocker.mock(FeedStreamJni.TEST_HOOKS, mFeedStreamJniMock);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mocker.mock(FeedProcessScopeDependencyProviderJni.TEST_HOOKS, mProcessScopeJniMock);
        mocker.mock(FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead()).thenReturn(5);

        // Profile/identity service set up.
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        SignInPromo.setDisablePromoForTests(true);

        // Preferences to enable feed.
        FeedSurfaceMediator.setPrefForTest(mPrefChangeRegistrar, mPrefService);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        // We want to make the feed service bridge ignore the ablation flag.
        when(mFeedServiceBridgeJniMock.isEnabled())
                .thenAnswer(invocation -> mPrefService.getBoolean(Pref.ENABLE_SNIPPETS));
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);
        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(true);

        // Resources set up.
        when(mSectionHeaderView.getResources()).thenReturn(mResources);
        when(mResources.getString(anyInt())).thenReturn("Test");

        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setAdapter(mAdapter);

        FeedServiceBridge.setProcessScopeForTesting(mProcessScope);

        when(mProcessScope.obtainSurfaceScope(any(SurfaceScopeDependencyProvider.class)))
                .thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideListRenderer()).thenReturn(mRenderer);
        when(mRenderer.bind(mContentManagerCaptor.capture(), isNull(), eq(false)))
                .thenReturn(mRecyclerView);
        when(mSurfaceScope.getFeedLaunchReliabilityLogger()).thenReturn(mLaunchReliabilityLogger);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTabModelSelector.getModel(eq(false))).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(eq(true))).thenReturn(mTabModelIncognito);

        mCoordinator = createCoordinator();

        mRecyclerView.setLayoutManager(mLayoutManager);

        // Print logs to stdout.
        ShadowLog.stream = System.out;
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        FeedSurfaceTracker.getInstance().resetForTest();
        IdentityServicesProvider.setInstanceForTests(null);
        FeedFeatures.setFakePrefsForTest(null);
        FeedSurfaceMediator.setPrefForTest(null, null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        FeedServiceBridge.setProcessScopeForTesting(null);
    }

    @Test
    public void testInactiveInitially() {
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, hasStreamBound());
    }

    @Test
    public void testActivate_startupNotCalled() {
        mCoordinator.onSurfaceOpened();

        // Calling to open the surface should not work because startup is not called.
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, hasStreamBound());
    }

    @Test
    public void testActivate_startupCalled() {
        FeedSurfaceTracker.getInstance().startup();

        // Startup should activate the coordinator and bind the feed.
        assertEquals(true, mCoordinator.isActive());
        assertEquals(true, hasStreamBound());
    }

    @Test
    public void testToggleSurfaceOpened() {
        FeedSurfaceTracker.getInstance().startup();
        mCoordinator.onSurfaceClosed();

        // Coordinator should be inactive because we closed the surface. Feed is unbound.
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, hasStreamBound());
    }

    @Test
    public void testActivate_feedHidden() {
        mCoordinator.getSectionHeaderModelForTest().set(
                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, false);
        FeedSurfaceTracker.getInstance().startup();

        // After startup, coordinator should be active, but feed should not be bound.
        assertEquals(true, mCoordinator.isActive());
        assertEquals(false, hasStreamBound());
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_webFeed() {
        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.WEB_FEED));
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_unknown() {
        assertEquals(FeedSurfaceCoordinator.StreamTabId.DEFAULT,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.UNKNOWN));
    }

    @Test
    public void testDisableReliabilityLogging_metricsReportingDisabled() {
        reset(mLaunchReliabilityLogger);
        mCoordinator.destroy();

        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(false);
        mCoordinator = createCoordinator();

        verify(mLaunchReliabilityLogger, never()).logUiStarting(anyInt(), anyLong());
    }

    @Test
    public void testLogUiStarting() {
        verify(mLaunchReliabilityLogger, times(1))
                .logUiStarting(SURFACE_TYPE, SURFACE_CREATION_TIME_NS);
    }

    @Test
    public void testActivityPaused() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mCoordinator.onActivityPaused();
        verify(mLaunchReliabilityLogger, times(1))
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.FRAGMENT_PAUSED.getNumber()));
    }

    @Test
    public void testActivityResumed() {
        mCoordinator.onActivityResumed();
        verify(mLaunchReliabilityLogger, times(1)).cancelPendingFinished();
    }

    @Test
    public void testOmniboxFocused() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mCoordinator.getReliabilityLogger().onOmniboxFocused();
        verify(mLaunchReliabilityLogger, times(1))
                .pendingFinished(anyLong(), eq(DiscoverLaunchResult.SEARCH_BOX_TAPPED.getNumber()));
    }

    @Test
    public void testVoiceSearch() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mCoordinator.getReliabilityLogger().onVoiceSearch();
        verify(mLaunchReliabilityLogger, times(1))
                .pendingFinished(
                        anyLong(), eq(DiscoverLaunchResult.VOICE_SEARCH_TAPPED.getNumber()));
    }

    @Test
    public void testUrlFocusChange() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mCoordinator.getReliabilityLogger().onUrlFocusChange(/*hasFocus=*/true);
        verify(mLaunchReliabilityLogger, never()).cancelPendingFinished();

        mCoordinator.getReliabilityLogger().onUrlFocusChange(/*hasFocus=*/false);
        verify(mLaunchReliabilityLogger, times(1)).cancelPendingFinished();
    }

    @Test
    public void testSetupHeaders_feedOn() {
        mCoordinator.setupHeaders(true);
        // Item count contains: feed header only
        assertEquals(1, mContentManagerCaptor.getValue().getItemCount());
    }

    @Test
    public void testSetupHeaders_feedOff() {
        mCoordinator.setupHeaders(false);
        // Item count contains: nothing, since ntp header is null
        assertEquals(0, mContentManagerCaptor.getValue().getItemCount());
    }

    @Test
    public void testLogManualRefresh() {
        mCoordinator.onRefresh();
        verify(mLaunchReliabilityLogger, times(1)).logManualRefresh(anyLong());
    }

    @Test
    public void testSetUpLaunchReliabilityLogger() {
        reset(mLaunchReliabilityLogger);
        mCoordinator.destroy();
        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(true);
        mCoordinator = createCoordinator();

        verify(mLaunchReliabilityLogger, times(1))
                .logUiStarting(SURFACE_TYPE, SURFACE_CREATION_TIME_NS);
    }

    @Test
    public void testFeedHeaderPosition_scrollableContainerDelegate() {
        when(mScrollableContainerDelegate.getTopPositionRelativeToContainerView(any()))
                .thenReturn(-1);
        assertEquals(-1, mCoordinator.getFeedHeaderPosition());

        mCoordinator.clearScrollableContainerDelegateForTesting();
        assertEquals(Integer.MAX_VALUE, mCoordinator.getFeedHeaderPosition());
    }

    @Test
    public void testStartSurfaceScrollListener() {
        FeedSurfaceCoordinator.StartSurfaceScrollListener listener =
                mCoordinator.new StartSurfaceScrollListener();

        // Our toolbar height is always set as 0.
        when(mCoordinator.getFeedHeaderPosition()).thenReturn(-10);
        listener.onHeaderOffsetChanged(0);
        // Toolbar height is bigger than the header position, then the sticky header is visible.
        assertEquals(true,
                mCoordinator.getSectionHeaderModelForTest().get(
                        SectionHeaderListProperties.STICKY_HEADER_VISIBLILITY_KEY));

        when(mCoordinator.getFeedHeaderPosition()).thenReturn(10);
        listener.onHeaderOffsetChanged(0);
        // Toolbar height is smaller than the header position, so the sticky header is invisible.
        assertEquals(false,
                mCoordinator.getSectionHeaderModelForTest().get(
                        SectionHeaderListProperties.STICKY_HEADER_VISIBLILITY_KEY));
    }

    private boolean hasStreamBound() {
        if (mCoordinator.getMediatorForTesting().getCurrentStreamForTesting() == null) {
            return false;
        }
        return ((FeedStream) mCoordinator.getMediatorForTesting().getCurrentStreamForTesting())
                .getBoundStatusForTest();
    }

    private FeedSurfaceCoordinator createCoordinator() {
        return new FeedSurfaceCoordinator(mActivity, mSnackbarManager, mWindowAndroid, mSnapHelper,
                null, 0, false, new TestSurfaceDelegate(), mProfileMock, false,
                mBottomSheetController, mShareDelegateSupplier, mScrollableContainerDelegate,
                NewTabPageLaunchOrigin.UNKNOWN, mPrivacyPreferencesManager,
                ()
                        -> { return null; },
                SURFACE_TYPE, SURFACE_CREATION_TIME_NS, null, false,
                /*viewportView=*/null, mFeedActionDelegate,
                /*helpAndFeedbackLauncher=*/null, mTabModelSelector);
    }
}
