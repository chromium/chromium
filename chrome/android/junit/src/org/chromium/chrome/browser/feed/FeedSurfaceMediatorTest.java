// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.Px;
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
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.sections.OnSectionHeaderSelectedListener;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderProperties;
import org.chromium.chrome.browser.feed.sections.ViewVisibility;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger.StreamType;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
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
// TODO(crbug.com/1353777): Disabling the feature explicitly, because native is not
// available to provide a default value. This should be enabled if the feature is enabled by
// default or removed if the flag is removed.
@Features.DisableFeatures(ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS)
@Features.EnableFeatures({ChromeFeatureList.WEB_FEED, ChromeFeatureList.INTEREST_FEED_V2_HEARTS,
        ChromeFeatureList.WEB_FEED_SORT, ChromeFeatureList.FEED_MULTI_COLUMN,
        ChromeFeatureList.FEED_HEADER_STICK_TO_TOP})
public class FeedSurfaceMediatorTest {
    static final @Px int TOOLBAR_HEIGHT = 10;
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
    private FeedStream mForYouStream;
    @Mock
    private FeedStream mFollowingStream;
    @Mock
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock
    private HybridListRenderer mHybridListRenderer;
    @Mock
    private ListLayoutHelper mListLayoutHelper;
    @Mock
    private FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    @Mock
    private FeedOptionsCoordinator mOptionsCoordinator;
    @Mock
    private FeedReliabilityLogger mReliabilityLogger;

    private Activity mActivity;
    private FeedSurfaceMediator mFeedSurfaceMediator;

    @Before
    public void setUp() {
        // Print logs to stdout.
        ShadowLog.stream = System.out;

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        // We want to make the feed service bridge ignore the ablation flag.
        when(mFeedServiceBridgeJniMock.isEnabled())
                .thenAnswer(invocation -> mPrefService.getBoolean(Pref.ENABLE_SNIPPETS));
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mFeedSurfaceCoordinator.isActive()).thenReturn(true);
        when(mFeedSurfaceCoordinator.getRecyclerView()).thenReturn(new RecyclerView(mActivity));
        when(mFeedSurfaceCoordinator.createFeedStream(
                     eq(StreamKind.FOLLOWING), any(Stream.StreamsMediator.class)))
                .thenReturn(mFollowingStream);
        when(mFeedSurfaceCoordinator.createFeedStream(
                     eq(StreamKind.FOR_YOU), any(Stream.StreamsMediator.class)))
                .thenReturn(mForYouStream);
        when(mFeedSurfaceCoordinator.getReliabilityLogger()).thenReturn(mReliabilityLogger);
        when(mReliabilityLogger.getLaunchLogger()).thenReturn(mLaunchReliabilityLogger);
        when(mFeedSurfaceCoordinator.getHybridListRenderer()).thenReturn(mHybridListRenderer);
        when(mHybridListRenderer.getListLayoutHelper()).thenReturn(mListLayoutHelper);
        when(mListLayoutHelper.setColumnCount(anyInt())).thenReturn(true);
        when(mFeedSurfaceCoordinator.getSurfaceLifecycleManager())
                .thenReturn(mFeedSurfaceLifecycleManager);
        ObservableSupplierImpl<Boolean> hasUnreadContent = new ObservableSupplierImpl<>();
        hasUnreadContent.set(false);
        when(mForYouStream.hasUnreadContent()).thenReturn(hasUnreadContent);
        when(mForYouStream.getStreamKind()).thenReturn(StreamKind.FOR_YOU);
        when(mFollowingStream.hasUnreadContent()).thenReturn(hasUnreadContent);
        when(mFollowingStream.getStreamKind()).thenReturn(StreamKind.FOLLOWING);

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
        FeedScrollState state = new FeedScrollState();
        state.tabId = 5;
        state.position = 2;
        state.lastPosition = 4;
        state.offset = 50;
        state.feedContentState = "foo";

        FeedScrollState deserializedState = FeedScrollState.fromJson(state.toJson());

        assertEquals(2, deserializedState.position);
        assertEquals(4, deserializedState.lastPosition);
        assertEquals(50, deserializedState.offset);
        assertEquals(5, deserializedState.tabId);
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
        assertEquals(state.tabId, deserializedState.tabId);
        assertEquals(state.feedContentState, deserializedState.feedContentState);
        assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testScrollStateFromInvalidJson() {
        assertEquals(null, FeedScrollState.fromJson("{{=xcg"));
    }

    @Test
    public void updateContent_openingTabIdFollowing() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        verify(mForYouStream, never())
                .bind(any(), any(), any(), any(), any(), any(), anyInt(), anyBoolean());
        verify(mFollowingStream, times(1))
                .bind(any(), any(), any(), any(), any(), any(), anyInt(), anyBoolean());
        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
    }

    @Test
    public void updateContent_openingTabIdForYou() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        verify(mForYouStream, times(1))
                .bind(any(), any(), any(), any(), any(), any(), anyInt(), anyBoolean());
        verify(mFollowingStream, never())
                .bind(any(), any(), any(), any(), any(), any(), anyInt(), anyBoolean());
        assertEquals(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(2, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_policyFeedOnOff() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();
        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();
        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertEquals(0, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_policyFeedOffOn() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(true);
        assertEquals(2, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_policyOff() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertEquals(0, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testOnSurfaceClosed_launchInProgress() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();

        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mReliabilityLogger, times(1))
                .logLaunchFinishedIfInProgress(
                        eq(DiscoverLaunchResult.FRAGMENT_STOPPED), eq(false));
    }

    @Test
    public void testOnSurfaceClosed_noLaunchInProgress() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator();
        mFeedSurfaceMediator.updateContent();

        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(false);
        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mReliabilityLogger, times(1))
                .logLaunchFinishedIfInProgress(
                        eq(DiscoverLaunchResult.FRAGMENT_STOPPED), eq(false));
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOn() {
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.INVISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(true, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.VISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testOnHeaderSelected_selectedWithOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        OnSectionHeaderSelectedListener listener =
                getOnSectionHeaderSelectedListener(model, forYou, true);
        listener.onSectionHeaderSelected(0);

        assertEquals(0, model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(false, forYou.get(SectionHeaderProperties.UNREAD_CONTENT_KEY));
        assertEquals(ViewVisibility.VISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
    }

    @Config(qualifiers = "en-sw600dp")
    @Test
    public void testOnHeaderSelected_selectedWithLatestOptionsOnTablet() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
        mFeedSurfaceMediator.updateContent();

        // Switch to following feed with latest option. Uses 2 column.
        when(mFollowingStream.supportsOptions()).thenReturn(true);
        when(mOptionsCoordinator.getSelectedOptionId()).thenReturn(ContentOrder.REVERSE_CHRON);
        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderSelected(1);
        verify(mListLayoutHelper, times(2)).setColumnCount(2);
    }

    @Config(qualifiers = "en-sw600dp")
    @Test
    public void testOnHeaderSelected_selectedWithSortOptionsOnTablet() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
        mFeedSurfaceMediator.updateContent();
        verify(mListLayoutHelper).setColumnCount(2);

        // Switch to following feed with sort option. Uses 1 column.
        when(mFollowingStream.supportsOptions()).thenReturn(true);
        when(mOptionsCoordinator.getSelectedOptionId()).thenReturn(ContentOrder.GROUPED);
        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderSelected(1);
        verify(mListLayoutHelper).setColumnCount(1);
    }

    @Config(qualifiers = "en-sw600dp")
    @Test
    public void testOnOptionSelectedOnTablet() {
        when(mFollowingStream.supportsOptions()).thenReturn(true);
        when(mOptionsCoordinator.getSelectedOptionId()).thenReturn(ContentOrder.GROUPED);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with Following feed with default sort option (1 column)
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
        mFeedSurfaceMediator.updateContent();
        verify(mListLayoutHelper).setColumnCount(1);

        // Switch to latest sort. Verify 2 column
        when(mOptionsCoordinator.getSelectedOptionId()).thenReturn(ContentOrder.REVERSE_CHRON);
        mFeedSurfaceMediator.onOptionChanged();
        verify(mListLayoutHelper).setColumnCount(2);
    }

    @Config(qualifiers = "en-sw600dp")
    @DisableFeatures(ChromeFeatureList.WEB_FEED_SORT)
    @Test
    public void testOnHeaderSelected_withFollowingAndSortDisabledOnTablet() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
        mFeedSurfaceMediator.updateContent();
        verify(mListLayoutHelper).setColumnCount(2);

        // Switch to following feed with sort option. Uses 1 column.
        when(mFollowingStream.supportsOptions()).thenReturn(true);
        when(mOptionsCoordinator.getSelectedOptionId()).thenReturn(ContentOrder.GROUPED);
        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderSelected(1);
        verify(mListLayoutHelper).setColumnCount(1);
    }

    private OnSectionHeaderSelectedListener getOnSectionHeaderSelectedListener(
            PropertyModel model, PropertyModel forYou, boolean value) {
        // Notes:
        // * The returned PropertyModel's will be configured to simulate the Following feed being
        //   the one selected.
        // * There's no point in returning PropertyModel's for the Following (or other) headers
        //   because individual Mediator onSectionHeader* will only affect the acted-on header. At
        //   runtime, many of them will be called in sequence to keep all headers in a consistent
        //   state.
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);
        forYou.set(SectionHeaderProperties.UNREAD_CONTENT_KEY, true);
        forYou.set(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY, ViewVisibility.GONE);

        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        model.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, 1);

        when(mForYouStream.supportsOptions()).thenReturn(value);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        return listener;
    }

    @Test
    public void testOnHeaderSelected_selectedNoOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        OnSectionHeaderSelectedListener listener =
                getOnSectionHeaderSelectedListener(model, forYou, false);
        listener.onSectionHeaderSelected(0);

        assertEquals(0, model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(false, forYou.get(SectionHeaderProperties.UNREAD_CONTENT_KEY));
        // Options Indicator untouched.
        assertEquals(ViewVisibility.GONE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
    }

    @Test
    public void testOnHeaderSelected_UnselectedWithOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);
        forYou.set(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY, ViewVisibility.GONE);

        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        when(mForYouStream.supportsOptions()).thenReturn(true);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderUnselected(0);

        assertEquals(ViewVisibility.INVISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
        verify(mOptionsCoordinator, times(1)).ensureGone();
    }

    @Test
    public void testOnHeaderSelected_UnselectedNoOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);
        forYou.set(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY, ViewVisibility.GONE);

        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        when(mForYouStream.supportsOptions()).thenReturn(false);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderUnselected(0);

        assertEquals(ViewVisibility.GONE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
        verify(mOptionsCoordinator, times(1)).ensureGone();
    }

    @Test
    public void testOnHeaderSelected_ReselectedWithOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        when(mForYouStream.supportsOptions()).thenReturn(true);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();

        listener.onSectionHeaderReselected(0);
        verify(mOptionsCoordinator, times(1)).toggleVisibility();
    }

    @Test
    public void testOnHeaderSelected_ReselectedNoOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);

        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        when(mForYouStream.supportsOptions()).thenReturn(false);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderReselected(0);

        verify(mOptionsCoordinator, never()).toggleVisibility();
    }

    @Test
    public void testOnHeaderSelected_showAndHideWithOptions() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(forYou);
        forYou.set(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY, ViewVisibility.GONE);
        forYou.set(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY, false);

        model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .add(SectionHeaderProperties.createSectionHeader("Following"));
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, model);

        when(mForYouStream.supportsOptions()).thenReturn(true);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING, mForYouStream);
        mFeedSurfaceMediator.setStreamForTesting(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU, mForYouStream);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        // Re-selects the header to show the options view.
        listener.onSectionHeaderReselected(0);
        verify(mOptionsCoordinator, times(1)).toggleVisibility();
        assertEquals(true, forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));

        listener.onSectionHeaderUnselected(0);
        // Verifies that unselecting header hides the options view and resets the
        // {@link SectionHeaderProperties.OPTIONS_VIEW_VISIBILITY_KEY}.
        assertEquals(ViewVisibility.INVISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
        verify(mOptionsCoordinator, times(1)).ensureGone();
        assertEquals(false, forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));

        // Re-selects the header to show the options view.
        listener.onSectionHeaderReselected(0);
        verify(mOptionsCoordinator, times(2)).toggleVisibility();
        assertEquals(true, forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));
        listener.onSectionHeaderReselected(0);
        // Verifies that re-selecting header also hides the options view and resets the
        // {@link SectionHeaderProperties.OPTIONS_VIEW_VISIBILITY_KEY}.
        verify(mOptionsCoordinator, times(3)).toggleVisibility();
        assertEquals(false, forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));
    }

    @Test
    public void testOnHeaderSelected_logSwitchedFeeds() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();
        reset(mReliabilityLogger);
        when(mReliabilityLogger.getLaunchLogger()).thenReturn(mLaunchReliabilityLogger);
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderSelected(0);

        verify(mReliabilityLogger, times(1))
                .logLaunchFinishedIfInProgress(
                        eq(DiscoverLaunchResult.SWITCHED_FEED_TABS), eq(false));
        verify(mLaunchReliabilityLogger, times(1))
                .logSwitchedFeeds(eq(StreamType.FOR_YOU), anyLong());
    }

    @Test
    public void testStreamsMediatorImpl_switchToStreamKind() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");
        OnSectionHeaderSelectedListener listener =
                getOnSectionHeaderSelectedListener(model, forYou, true);

        Stream.StreamsMediator streamsMediator = mFeedSurfaceMediator.new StreamsMediatorImpl();
        streamsMediator.switchToStreamKind(StreamKind.FOR_YOU);

        assertEquals(0, model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(false, forYou.get(SectionHeaderProperties.UNREAD_CONTENT_KEY));
        assertEquals(ViewVisibility.VISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
    }

    private FeedSurfaceMediator createMediator() {
        return createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
    }

    private FeedSurfaceMediator createMediator(
            @FeedSurfaceCoordinator.StreamTabId int tabId, PropertyModel sectionHeaderModel) {
        return new FeedSurfaceMediator(mFeedSurfaceCoordinator, mActivity, null, sectionHeaderModel,
                tabId, /*actionDelegate=*/null, mOptionsCoordinator);
    }
}
