// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

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
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Locale;
import java.util.function.Supplier;

/** Tests for {@link FeedSurfaceCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({
    ChromeFeatureList.WEB_FEED_SORT,
    ChromeFeatureList.WEB_FEED_ONBOARDING,
    ChromeFeatureList.FEED_CONTAINMENT,
    ChromeFeatureList.FEED_HEADER_REMOVAL
})
@EnableFeatures({ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP, SigninFeatures.ENABLE_SEAMLESS_SIGNIN})
public class FeedSurfaceCoordinatorTest {
    private static final @SurfaceType int SURFACE_TYPE = SurfaceType.NEW_TAB_PAGE;
    private static final long SURFACE_CREATION_TIME_NS = 1234L;
    private BackgroundImageInfo mBackgroundImageInfo;
    private Bitmap mBitmap;

    @Mock private NtpBackgroundImageCoordinator mBackgroundImageCoordinator;

    private static class TestLifecycleManager extends FeedSurfaceLifecycleManager {
        public TestLifecycleManager(Activity activity, FeedSurfaceCoordinator coordinator) {
            super(activity, coordinator);
        }

        @Override
        public boolean canShow() {
            return true;
        }
    }

    private static class TestSurfaceDelegate implements FeedSurfaceDelegate {
        @Override
        public FeedSurfaceLifecycleManager createStreamLifecycleManager(
                Activity activity, SurfaceCoordinator coordinator, Profile profile) {
            TestLifecycleManager lifecycleManager =
                    new TestLifecycleManager(activity, (FeedSurfaceCoordinator) coordinator);
            return lifecycleManager;
        }

        @Override
        public void sendMotionEventForInputTracking(MotionEvent ev) {}

        @Override
        public boolean onInterceptTouchEvent(MotionEvent e) {
            return false;
        }
    }

    private static class TestTabModel extends EmptyTabModel {
        public final ArrayList<TabModelObserver> mObservers = new ArrayList<>();

        @Override
        public void addObserver(TabModelObserver observer) {
            mObservers.add(observer);
        }
    }

    private final TestTabModel mTabModel = new TestTabModel();
    private final TestTabModel mTabModelIncognito = new TestTabModel();

    private FeedSurfaceCoordinator mCoordinator;

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private LinearLayoutManager mLayoutManager;

    // Mocked Direct dependencies.
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnapScrollHelper mSnapHelper;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private FeedActionDelegate mFeedActionDelegate;

    // Mocked JNI.
    @Mock private FeedSurfaceRendererBridge.Natives mFeedSurfaceRendererBridgeJniMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private FeedProcessScopeDependencyProvider.Natives mProcessScopeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    // Mocked xSurface setup.
    @Mock private ProcessScope mProcessScope;
    @Mock private FeedSurfaceScope mSurfaceScope;
    @Mock private HybridListRenderer mRenderer;
    @Captor private ArgumentCaptor<FeedListContentManager> mContentManagerCaptor;

    // Mocked indirect dependencies.
    @Mock private Profile mProfileMock;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private PrefService mPrefService;
    @Mock private TemplateUrlService mUrlService;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock private Tracker mTracker;
    @Mock private ScrollableContainerDelegate mScrollableContainerDelegate;
    @Mock ObservableSupplier<Integer> mTabStripHeightSupplier;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mEdgePadAdjusterCaptor;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FeedSurfaceMediator mMediatorSpy;
    private int mTabStripHeight;
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        Configuration config = new Configuration();
        config.setLocale(new Locale("en", "US"));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mLayoutManager = new LinearLayoutManager(mActivity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FeedSurfaceRendererBridgeJni.setInstanceForTesting(mFeedSurfaceRendererBridgeJniMock);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJniMock);
        FeedProcessScopeDependencyProviderJni.setInstanceForTesting(mProcessScopeJniMock);
        FeedReliabilityLoggingBridgeJni.setInstanceForTesting(mFeedReliabilityLoggingBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead()).thenReturn(5);

        // Profile/identity service set up.
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mIdentityService.getIdentityManager(any(Profile.class))).thenReturn(mIdentityManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        SignInPromo.setDisablePromoForTesting(true);

        // Preferences to enable feed.
        FeedSurfaceMediator.setPrefForTest(mPrefChangeRegistrar, mPrefService);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        // We want to make the feed service bridge ignore the ablation flag.
        when(mFeedServiceBridgeJniMock.isEnabled())
                .thenAnswer(invocation -> mPrefService.getBoolean(Pref.ENABLE_SNIPPETS));
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);
        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(true);
        when(mUserPrefsJniMock.get(any(Profile.class))).thenReturn(mPrefService);

        // Resources set up.

        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setAdapter(mAdapter);

        XSurfaceProcessScopeProvider.setProcessScopeForTesting(mProcessScope);

        when(mProcessScope.obtainFeedSurfaceScope(any(FeedSurfaceScopeDependencyProvider.class)))
                .thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideListRenderer()).thenReturn(mRenderer);
        when(mSurfaceScope.getLaunchReliabilityLogger()).thenReturn(mLaunchReliabilityLogger);
        TrackerFactory.setTrackerForTests(mTracker);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        mTabStripHeight = mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        when(mTabStripHeightSupplier.get()).thenReturn(mTabStripHeight);

        mCoordinator = createCoordinator(mRecyclerView);

        mRecyclerView.setLayoutManager(mLayoutManager);

        mMediatorSpy = Mockito.spy(mCoordinator.getMediatorForTesting());
        mCoordinator.setMediatorForTesting(mMediatorSpy);

        // Print logs to stdout.
        ShadowLog.stream = System.out;

        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBackgroundImageInfo = new BackgroundImageInfo(new Matrix(), new Matrix());
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        FeedSurfaceTracker.getInstance().resetForTest();
        FeedSurfaceMediator.setPrefForTest(null, null);
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
        mCoordinator
                .getSectionHeaderModelForTest()
                .set(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, false);
        FeedSurfaceTracker.getInstance().startup();

        // After startup, coordinator should be active, but feed should not be bound.
        assertEquals(true, mCoordinator.isActive());
        assertEquals(false, hasStreamBound());
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_webFeed() {
        assertEquals(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.WEB_FEED));
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_unknown() {
        assertEquals(
                FeedSurfaceCoordinator.StreamTabId.DEFAULT,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.UNKNOWN));
    }

    @Test
    public void testDisableReliabilityLogging_metricsReportingDisabled() {
        reset(mLaunchReliabilityLogger);
        mCoordinator.destroy();

        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(false);
        mCoordinator = createCoordinator(mRecyclerView);

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
        mCoordinator.getReliabilityLogger().onUrlFocusChange(/* hasFocus= */ true);
        verify(mLaunchReliabilityLogger, never()).cancelPendingFinished();

        mCoordinator.getReliabilityLogger().onUrlFocusChange(/* hasFocus= */ false);
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
    public void testNonSwipeRefresh() {
        mCoordinator.nonSwipeRefresh();
        verify(mMediatorSpy).manualRefresh(any());
        verify(mLaunchReliabilityLogger, times(1)).logManualRefresh(anyLong());
    }

    @Test
    public void testOnRefresh() {
        mCoordinator.onRefresh();
        verify(mMediatorSpy).manualRefresh(any());
        verify(mLaunchReliabilityLogger, times(1)).logManualRefresh(anyLong());
    }

    @Test
    public void testReload() {
        mCoordinator.reload();
        verify(mMediatorSpy).manualRefresh(any());
        verify(mLaunchReliabilityLogger, times(1)).logManualRefresh(anyLong());
    }

    @Test
    public void testSetUpLaunchReliabilityLogger() {
        reset(mLaunchReliabilityLogger);
        mCoordinator.destroy();
        when(mPrivacyPreferencesManager.isMetricsReportingEnabled()).thenReturn(true);
        mCoordinator = createCoordinator(mRecyclerView);

        verify(mLaunchReliabilityLogger, times(1))
                .logUiStarting(SURFACE_TYPE, SURFACE_CREATION_TIME_NS);
    }

    @Test
    public void testTabStripHeightChangeCallback() {
        ArgumentCaptor<Callback<Integer>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mTabStripHeightSupplier).addObserver(captor.capture());
        Callback<Integer> tabStripHeightChangeCallback = captor.getValue();
        tabStripHeightChangeCallback.onResult(mTabStripHeight);
        assertEquals(
                "Top padding of root view should be updated when tab strip height changes.",
                mTabStripHeight,
                mCoordinator.getRootViewForTesting().getPaddingTop());
    }

    @Test
    public void testEdgeToEdge() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mEdgePadAdjusterCaptor.capture());

        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(100);
        assertFalse(
                "Recycler view should clip to padding when edge to edge.",
                mCoordinator.getRecyclerView().getClipToPadding());
        assertEquals(
                "Padding is different.", 100, mCoordinator.getRecyclerView().getPaddingBottom());

        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(0);
        assertTrue(
                "Recycler view should no longer clip to padding when not drawing to edge.",
                mCoordinator.getRecyclerView().getClipToPadding());
        assertEquals(
                "Padding should be reset.", 0, mCoordinator.getRecyclerView().getPaddingBottom());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FEED_HEADER_REMOVAL + ":treatment/label")
    public void testFeedHeaderShownWithLabelOnly() {
        assertEquals(View.VISIBLE, mCoordinator.getHeaderViewForTesting().getVisibility());
        assertEquals(0, mCoordinator.getHeaderPosition());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FEED_HEADER_REMOVAL + ":treatment/none")
    public void testFeedHeaderHidden() {
        assertEquals(View.GONE, mCoordinator.getHeaderViewForTesting().getVisibility());
        assertEquals(1, mCoordinator.getHeaderPosition());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testSetBackground_withImageFromDisk_delegatesToView() {
        mCoordinator.setBackgroundImageCoordinatorForTesting(mBackgroundImageCoordinator);
        NtpCustomizationConfigManager configManager = NtpCustomizationConfigManager.getInstance();
        configManager.setBackgroundImageTypeForTesting(CHROME_COLOR);

        configManager.onUploadedImageSelected(mBitmap, mBackgroundImageInfo);

        // Verifies the coordinator delegates the setBackground call to the custom view.
        verify(mBackgroundImageCoordinator)
                .setBackground(eq(mBitmap), eq(mBackgroundImageInfo), eq(IMAGE_FROM_DISK));
        configManager.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FLUID_RESIZE)
    @Config(qualifiers = "sw800dp-w800dp-h1200dp") // More specific tablet config
    public void testResize_onTablet_withFeatureEnabled_takesSnapshot() throws Exception {
        View rootView = mCoordinator.getRootViewForTesting();
        mActivity.setContentView(rootView);
        RecyclerView recyclerView = mCoordinator.getRecyclerView();
        recyclerView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        ImageView snapshotOverlay = mCoordinator.getRecyclerViewSnapshotOverlayForTesting();
        assertNotNull("Snapshot overlay should not be null", snapshotOverlay);

        // Force a measure and layout pass to ensure the view has dimensions
        // and is ready for snapshotting.
        int oldWidth = 500;
        int oldHeight = 400;
        rootView.measure(
                View.MeasureSpec.makeMeasureSpec(oldWidth, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(oldHeight, View.MeasureSpec.EXACTLY));
        rootView.layout(0, 0, oldWidth, oldHeight);
        ShadowLooper.runUiThreadTasks();

        // Verify that the RecyclerView has been laid out with the correct dimensions before
        // resizing.
        assertEquals(
                "RecyclerView should have the initial width", oldWidth, recyclerView.getWidth());
        assertEquals(
                "RecyclerView should have the initial height",
                oldHeight - mTabStripHeight,
                recyclerView.getHeight());

        // Initial state: RecyclerView visible, snapshot overlay gone.
        assertEquals(View.VISIBLE, recyclerView.getVisibility());
        assertEquals(View.GONE, snapshotOverlay.getVisibility());

        // Simulate a resize event using reflection.
        int newWidth = 1000;
        int newHeight = 800;
        Method onSizeChanged =
                View.class.getDeclaredMethod(
                        "onSizeChanged", int.class, int.class, int.class, int.class);
        onSizeChanged.setAccessible(true);
        onSizeChanged.invoke(rootView, newWidth, newHeight, oldWidth, oldHeight);

        // Immediately after resize, snapshot overlay should be visible, RecyclerView invisible.
        assertEquals(View.INVISIBLE, recyclerView.getVisibility());
        assertEquals(View.VISIBLE, snapshotOverlay.getVisibility());
        // Let the posted tasks run, which should hide the overlay and show the RecyclerView.
        ShadowLooper.runUiThreadTasks();

        // After the delay, RecyclerView should be visible again, and the snapshot overlay gone.
        assertEquals(View.VISIBLE, recyclerView.getVisibility());
        assertEquals(View.GONE, snapshotOverlay.getVisibility());
    }

    private boolean hasStreamBound() {
        if (mCoordinator.getMediatorForTesting().getCurrentStreamForTesting() == null) {
            return false;
        }
        return ((FeedStream) mCoordinator.getMediatorForTesting().getCurrentStreamForTesting())
                .isBound();
    }

    private FeedSurfaceCoordinator createCoordinator(RecyclerView recyclerview) {
        when(mRenderer.bind(mContentManagerCaptor.capture(), isNull(), anyInt()))
                .thenReturn(recyclerview);
        when(mRenderer.getAdapter()).thenReturn(mAdapter);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        return new FeedSurfaceCoordinator(
                mActivity,
                mSnackbarManager,
                mWindowAndroid,
                mSnapHelper,
                null,
                0,
                false,
                new TestSurfaceDelegate(),
                mProfileMock,
                mBottomSheetController,
                mShareDelegateSupplier,
                mScrollableContainerDelegate,
                NewTabPageLaunchOrigin.UNKNOWN,
                mPrivacyPreferencesManager,
                () -> {
                    return null;
                },
                SURFACE_CREATION_TIME_NS,
                null,
                false,
                /* viewportView= */ null,
                mFeedActionDelegate,
                mTabStripHeightSupplier,
                mEdgeToEdgeSupplier);
    }
}
