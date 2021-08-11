// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.v2.FeedStream;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderListProperties;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link FeedSurfaceMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.WEB_FEED, ChromeFeatureList.INTEREST_FEED_V2_HEARTS})
public class FeedSurfaceMediatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();

    // Mocked JNI.
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;

    @Mock
    private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    @Mock
    private IdentityServicesProvider mIdentityService;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock
    private PrefService mPrefService;
    @Mock
    private Profile mProfileMock;
    @Mock
    private SigninManager mSigninManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private TemplateUrlService mUrlService;
    @Mock
    private FeedStream mStream;
    @Mock
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock
    private HybridListRenderer mHybridListRenderer;
    @Mock
    private FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;

    private Activity mActivity;
    private FeedSurfaceMediator mFeedSurfaceMediator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_UI)).thenReturn(true);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mFeedSurfaceCoordinator.getRecyclerView()).thenReturn(new RecyclerView(mActivity));
        when(mFeedSurfaceCoordinator.getStream()).thenReturn(mStream);
        when(mFeedSurfaceCoordinator.createFeedStream(anyBoolean())).thenReturn(mStream);
        when(mFeedSurfaceCoordinator.getLaunchReliabilityLogger())
                .thenReturn(mLaunchReliabilityLogger);
        when(mFeedSurfaceCoordinator.getHybridListRenderer()).thenReturn(mHybridListRenderer);
        when(mFeedSurfaceCoordinator.getSurfaceLifecycleManager())
                .thenReturn(mFeedSurfaceLifecycleManager);
        ObservableSupplierImpl<Boolean> hasUnreadContent = new ObservableSupplierImpl<>();
        hasUnreadContent.set(false);
        when(mStream.hasUnreadContent()).thenReturn(hasUnreadContent);

        FeedSurfaceMediator.setPrefForTest(mPrefChangeRegistrar, mPrefService);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);
        SignInPromo.setDisablePromoForTests(true);
    }

    @After
    public void tearDown() {
        if (mFeedSurfaceMediator != null) mFeedSurfaceMediator.destroy();
        FeedSurfaceMediator.setPrefForTest(null, null);
        FeedFeatures.setFakePrefsForTest(null);
        Profile.setLastUsedProfileForTesting(null);
        IdentityServicesProvider.setInstanceForTests(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        SignInPromo.setDisablePromoForTests(false);
    }

    @Test
    public void testSerializeScrollState() {
        FeedSurfaceMediator.ScrollState state = new FeedSurfaceMediator.ScrollState();
        state.tabId = 5;
        state.position = 2;
        state.lastPosition = 4;
        state.offset = 50;

        FeedSurfaceMediator.ScrollState deserializedState =
                FeedSurfaceMediator.ScrollState.fromJson(state.toJson());

        assertEquals(2, deserializedState.position);
        assertEquals(4, deserializedState.lastPosition);
        assertEquals(50, deserializedState.offset);
        assertEquals(5, deserializedState.tabId);
        assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testScrollStateFromInvalidJson() {
        assertEquals(null, FeedSurfaceMediator.ScrollState.fromJson("{{=xcg"));
    }

    @Test
    public void updateContent_openingTabIdFollowing() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create();
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
    }

    @Test
    public void updateContent_openingTabIdForYou() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create();
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
    }

    @Test
    public void testOnSurfaceClosed_launchInProgress() {
        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.bindStream(mStream);

        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mLaunchReliabilityLogger, times(1))
                .logLaunchFinished(
                        anyLong(), eq(DiscoverLaunchResult.FRAGMENT_STOPPED.getNumber()));
    }

    @Test
    public void testOnSurfaceClosed_noLaunchInProgress() {
        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.bindStream(mStream);

        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(false);
        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mLaunchReliabilityLogger, never()).logLaunchFinished(anyLong(), anyInt());
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOn() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.INVISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOff() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.VISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOn() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOff() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(true, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.VISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.VISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create();
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(SectionHeaderListProperties.ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    private FeedSurfaceMediator createMediator() {
        return createMediator(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, SectionHeaderListProperties.create());
    }

    private FeedSurfaceMediator createMediator(
            @FeedSurfaceCoordinator.StreamTabId int tabId, PropertyModel sectionHeaderModel) {
        return new FeedSurfaceMediator(mFeedSurfaceCoordinator, mActivity, null,
                mPageNavigationDelegate, sectionHeaderModel, tabId);
    }
}
