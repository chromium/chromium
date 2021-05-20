// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.MotionEvent;

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
import org.chromium.chrome.browser.AppHooksImpl;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.v2.FakeLinearLayoutManager;
import org.chromium.chrome.browser.feed.v2.FeedStream;
import org.chromium.chrome.browser.feed.v2.FeedStreamJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderListProperties;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests for {@link FeedSurfaceCoordinator}.
 *
 * EnhancedProtectionPromoCard does not need to be disabled. Its value just need to be set.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures({ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
        ChromeFeatureList.WEB_FEED, ChromeFeatureList.INTEREST_FEED_V2_AUTOPLAY})
public class FeedSurfaceCoordinatorTest {
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
                Activity activity, FeedSurfaceCoordinator coordinator) {
            mLifecycleManager = new TestLifecycleManager(activity, coordinator);
            return mLifecycleManager;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent e) {
            return false;
        }
    }

    private FeedSurfaceCoordinator mCoordinator;

    @Rule
    public JniMocker mocker = new JniMocker();

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
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
    private TabModelSelector mTabModelSelector;

    // Mocked JNI.
    @Mock
    private FeedStream.Natives mFeedStreamJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;

    // Mocked xSurface setup.
    @Mock
    private AppHooksImpl mApphooks;
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

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedStreamJni.TEST_HOOKS, mFeedStreamJniMock);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);

        // Resources set up.
        when(mSectionHeaderView.getResources()).thenReturn(mResources);
        when(mResources.getString(anyInt())).thenReturn("Test");

        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setAdapter(mAdapter);

        // XSurface setup.
        when(mApphooks.getExternalSurfaceProcessScope(any(ProcessScopeDependencyProvider.class)))
                .thenReturn(mProcessScope);
        when(mProcessScope.obtainSurfaceScope(any(SurfaceScopeDependencyProvider.class)))
                .thenReturn(mSurfaceScope);
        when(mSurfaceScope.provideListRenderer()).thenReturn(mRenderer);
        when(mRenderer.bind(mContentManagerCaptor.capture())).thenReturn(mRecyclerView);
        AppHooksImpl.setInstanceForTesting(mApphooks);

        mCoordinator = new FeedSurfaceCoordinator(mActivity, mSnackbarManager, mWindowAndroid,
                mSnapHelper, null, mSectionHeaderView, false, new TestSurfaceDelegate(),
                mPageNavigationDelegate, mProfileMock, false, mBottomSheetController,
                mShareDelegateSupplier, null, mTabModelSelector, NewTabPageLaunchOrigin.UNKNOWN);

        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);

        // Print logs to stdout.
        ShadowLog.stream = System.out;
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        FeedSurfaceTracker.getInstance().resetForTest();
        AppHooksImpl.setInstanceForTesting(null);
        IdentityServicesProvider.setInstanceForTests(null);
        FeedFeatures.setFakePrefsForTest(null);
        FeedSurfaceMediator.setPrefForTest(null, null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
    }

    @Test
    public void testInactiveInitially() {
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, ((FeedStream) mCoordinator.getStream()).getBoundStatusForTest());
    }

    @Test
    public void testActivate_startupNotCalled() {
        mCoordinator.onSurfaceOpened();

        // Calling to open the surface should not work because startup is not called.
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, ((FeedStream) mCoordinator.getStream()).getBoundStatusForTest());
    }

    @Test
    public void testActivate_startupCalled() {
        FeedSurfaceTracker.getInstance().startup();

        // Startup should activate the coordinator and bind the feed.
        assertEquals(true, mCoordinator.isActive());
        assertEquals(true, ((FeedStream) mCoordinator.getStream()).getBoundStatusForTest());
    }

    @Test
    public void testToggleSurfaceOpened() {
        FeedSurfaceTracker.getInstance().startup();
        mCoordinator.onSurfaceClosed();

        // Coordinator should be inactive because we closed the surface. Feed is unbound.
        assertEquals(false, mCoordinator.isActive());
        assertEquals(false, ((FeedStream) mCoordinator.getStream()).getBoundStatusForTest());
    }

    @Test
    public void testActivate_feedHidden() {
        mCoordinator.getSectionHeaderModelForTest().set(
                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, false);
        FeedSurfaceTracker.getInstance().startup();

        // After startup, coordinator should be active, but feed should not be bound.
        assertEquals(true, mCoordinator.isActive());
        assertEquals(false, ((FeedStream) mCoordinator.getStream()).getBoundStatusForTest());
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_webFeed() {
        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.WEB_FEED));
    }

    @Test
    public void testGetTabIdFromLaunchOrigin_unknown() {
        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                mCoordinator.getTabIdFromLaunchOrigin(NewTabPageLaunchOrigin.UNKNOWN));
    }
}
