// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

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
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.sections.OnSectionHeaderSelectedListener;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderProperties;
import org.chromium.chrome.browser.feed.sections.ViewVisibility;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.browser.xsurface.feed.StreamType;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link FeedSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.INTEREST_FEED_V2_HEARTS,
    ChromeFeatureList.WEB_FEED_SORT,
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
})
public class FeedSurfaceMediatorTest {
    static final @Px int TOOLBAR_HEIGHT = 10;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mocker = new JniMocker();

    // Mocked JNI.
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;

    @Mock private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfileMock;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TemplateUrlService mUrlService;
    @Mock private FeedStream mForYouStream;
    @Mock private FeedStream mFollowingStream;
    @Mock private FeedStream mSupervisedUserStream;
    @Mock private HybridListRenderer mHybridListRenderer;
    @Mock private ListLayoutHelper mListLayoutHelper;
    @Mock private FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    @Mock private FeedOptionsCoordinator mOptionsCoordinator;
    @Mock private FeedReliabilityLogger mReliabilityLogger;
    @Mock private UiConfig mUiConfig;
    @Captor private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserverCaptor;
    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverCaptor;
    private final Context mContext = RuntimeEnvironment.application;
    private Activity mActivity;
    private FeedSurfaceMediator mFeedSurfaceMediator;

    @Before
    public void setUp() {
        // Print logs to stdout.
        ShadowLog.stream = System.out;

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // We want to make the feed service bridge ignore the ablation flag.
        when(mFeedServiceBridgeJniMock.isEnabled())
                .thenAnswer(invocation -> mPrefService.getBoolean(Pref.ENABLE_SNIPPETS));
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(true);
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
        when(mFeedSurfaceCoordinator.createFeedStream(
                        eq(StreamKind.SUPERVISED_USER), any(Stream.StreamsMediator.class)))
                .thenReturn(mSupervisedUserStream);
        when(mFeedSurfaceCoordinator.getReliabilityLogger()).thenReturn(mReliabilityLogger);
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
        when(mSupervisedUserStream.hasUnreadContent()).thenReturn(hasUnreadContent);
        when(mSupervisedUserStream.getStreamKind()).thenReturn(StreamKind.SUPERVISED_USER);

        FeedSurfaceMediator.setPrefForTest(mPrefChangeRegistrar, mPrefService);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        TemplateUrlServiceFactory.setInstanceForTesting(mUrlService);
        SignInPromo.setDisablePromoForTesting(true);
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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        verify(mForYouStream, never()).bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mFollowingStream, times(1)).bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mSupervisedUserStream, never())
                .bind(any(), any(), any(), any(), any(), any(), anyInt());
        assertEquals(
                FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
    }

    @Test
    public void updateContent_openingTabIdForYou() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();

        verify(mForYouStream, times(1)).bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mFollowingStream, never()).bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mSupervisedUserStream, never())
                .bind(any(), any(), any(), any(), any(), any(), anyInt());

        assertEquals(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                sectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(2, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void updateContent_openingTabIdSupervisedUser() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        verify(mSupervisedUserStream, times(1))
                .bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mForYouStream, never()).bind(any(), any(), any(), any(), any(), any(), anyInt());
        verify(mFollowingStream, never()).bind(any(), any(), any(), any(), any(), any(), anyInt());
        assertEquals(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
    }

    @Test
    public void testMenuItems_supervisedFeedOn() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        MVCListAdapter.ModelList menuItemList =
                model.get(SectionHeaderListProperties.MENU_MODEL_LIST_KEY);

        assertEquals(2, menuItemList.size());
        assertEquals(
                R.string.learn_more,
                menuItemList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.ntp_turn_off_feed,
                menuItemList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testMenuItems_supervisedFeedOff() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        MVCListAdapter.ModelList menuItemList =
                model.get(SectionHeaderListProperties.MENU_MODEL_LIST_KEY);

        assertTrue(menuItemList.size() > 2);
    }

    @Test
    public void testHeaderText_supervisedFeedOnWithDefaultSearchEngineGoogle() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        doReturn(true).when(mUrlService).isDefaultSearchEngineGoogle();

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        assertEquals(
                model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .get(0)
                        .get(SectionHeaderProperties.HEADER_TEXT_KEY),
                mContext.getResources().getString(R.string.supervised_user_ntp_discover_on));
    }

    @Test
    public void testHeaderText_supervisedFeedOffWithDefaultSearchEngineGoogle() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        doReturn(true).when(mUrlService).isDefaultSearchEngineGoogle();

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        assertEquals(
                model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .get(0)
                        .get(SectionHeaderProperties.HEADER_TEXT_KEY),
                mContext.getResources().getString(R.string.supervised_user_ntp_discover_off));
    }

    @Test
    public void testHeaderText_supervisedFeedOnWithDefaultSearchEngineNotGoogle() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        doReturn(false).when(mUrlService).isDefaultSearchEngineGoogle();

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        assertEquals(
                model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .get(0)
                        .get(SectionHeaderProperties.HEADER_TEXT_KEY),
                mContext.getResources()
                        .getString(R.string.supervised_user_ntp_discover_on_branded));
    }

    @Test
    public void testHeaderText_supervisedFeedOffWithDefaultSearchEngineNotGoogle() {
        when(mFeedSurfaceCoordinator.shouldDisplaySupervisedFeed()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        doReturn(false).when(mUrlService).isDefaultSearchEngineGoogle();

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();

        assertEquals(
                model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .get(0)
                        .get(SectionHeaderProperties.HEADER_TEXT_KEY),
                mContext.getResources()
                        .getString(R.string.supervised_user_ntp_discover_off_branded));
    }

    @Test
    public void testUpdateContent_policyFeedOnOff() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertEquals(0, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_DseFeedOnOff() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();
        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertEquals(0, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_DseFeedOffOn() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        // Turn feed on.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(true);
        assertEquals(2, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testUpdateContent_DseOff() {
        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOLLOWING, sectionHeaderModel);
        mFeedSurfaceMediator.onSurfaceOpened();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Turn feed off.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);
        mFeedSurfaceMediator.updateContent();

        verify(mFeedSurfaceCoordinator).setupHeaders(false);
        assertEquals(0, mFeedSurfaceMediator.getTabToStreamSizeForTesting());
    }

    @Test
    public void testObserveTemplateUrlService() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        doReturn(true).when(mUrlService).isDefaultSearchEngineGoogle();

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
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
    public void testWithEeaCountryOnlyEnabled() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        doReturn(false).when(mUrlService).isDefaultSearchEngineGoogle();
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(false);

        // Verifies that Feeds is enabled if the device isn't from an EEA country.
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        verify(mPrefService).setBoolean(eq(Pref.ENABLE_SNIPPETS_BY_DSE), eq(true));

        // Verifies that Feeds is disabled if the device is from an EEA country.
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        verify(mPrefService).setBoolean(eq(Pref.ENABLE_SNIPPETS_BY_DSE), eq(false));
    }

    @Test
    public void testOnSurfaceClosed() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        mFeedSurfaceMediator = createMediator();
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();

        mFeedSurfaceMediator.onSurfaceClosed();
        verify(mForYouStream).unbind(anyBoolean(), anyBoolean());
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOn() {
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.INVISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(true, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(true, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.VISIBLE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedInNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOn() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
                model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateSectionHeader_signedOutNonGseOff() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);

        mFeedSurfaceMediator = createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, model);
        mFeedSurfaceMediator.updateContent();
        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(false);
        when(mUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        mFeedSurfaceMediator.updateSectionHeader();

        assertEquals(false, model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        assertEquals(false, model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        assertEquals(
                ViewVisibility.GONE,
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
        assertEquals(
                ViewVisibility.VISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
    }

    @Config(qualifiers = "en-sw600dp")
    @Test
    public void testOnHeaderSelected_selectedWithLatestOptionsOnTablet() {
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with Following feed with default sort option (1 column)
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOLLOWING,
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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        // Set up mediator with For_you feed (2 column)
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
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
        assertEquals(
                ViewVisibility.GONE,
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

        assertEquals(
                ViewVisibility.INVISIBLE,
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

        assertEquals(
                ViewVisibility.GONE,
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
        assertEquals(
                ViewVisibility.INVISIBLE,
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
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS)).thenReturn(true);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel);
        mFeedSurfaceMediator.updateContent();
        reset(mReliabilityLogger);

        OnSectionHeaderSelectedListener listener =
                mFeedSurfaceMediator.getOrCreateSectionHeaderListenerForTesting();
        listener.onSectionHeaderSelected(0);

        verify(mReliabilityLogger, times(1)).onSwitchStream(eq(StreamType.FOR_YOU));
    }

    @Test
    public void testStreamsMediatorImpl_switchToStreamKind() {
        PropertyModel model = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        PropertyModel forYou = SectionHeaderProperties.createSectionHeader("For you");

        getOnSectionHeaderSelectedListener(model, forYou, true);

        Stream.StreamsMediator streamsMediator = mFeedSurfaceMediator.new StreamsMediatorImpl();
        streamsMediator.switchToStreamKind(StreamKind.FOR_YOU);

        assertEquals(0, model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        assertEquals(false, forYou.get(SectionHeaderProperties.UNREAD_CONTENT_KEY));
        assertEquals(
                ViewVisibility.VISIBLE,
                forYou.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
    }

    @Test
    public void testUpdateHeaderWithUiConfigChanged() {
        // Sets the current display style to be the default wide window.
        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel, mUiConfig);
        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        assertFalse(
                sectionHeaderModel.get(SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY));

        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(displayStyleRegular);
        assertTrue(
                sectionHeaderModel.get(SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY));

        mFeedSurfaceMediator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserverCaptor.capture());
    }

    @Test
    public void testInitializeHeaderWithCurrentUiConfig() {
        // Sets the current display style to be a narrow window.
        UiConfig.DisplayStyle displayStylRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStylRegular);

        PropertyModel sectionHeaderModel = SectionHeaderListProperties.create(TOOLBAR_HEIGHT);
        mFeedSurfaceMediator =
                createMediator(
                        FeedSurfaceCoordinator.StreamTabId.FOR_YOU, sectionHeaderModel, mUiConfig);

        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        assertTrue(
                sectionHeaderModel.get(SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY));
    }

    private FeedSurfaceMediator createMediator() {
        return createMediator(
                FeedSurfaceCoordinator.StreamTabId.FOR_YOU,
                SectionHeaderListProperties.create(TOOLBAR_HEIGHT));
    }

    private FeedSurfaceMediator createMediator(
            @FeedSurfaceCoordinator.StreamTabId int tabId, PropertyModel sectionHeaderModel) {
        return createMediator(tabId, sectionHeaderModel, /* uiConfig= */ null);
    }

    private FeedSurfaceMediator createMediator(
            @FeedSurfaceCoordinator.StreamTabId int tabId,
            PropertyModel sectionHeaderModel,
            UiConfig uiConfig) {
        return new FeedSurfaceMediator(
                mFeedSurfaceCoordinator,
                mActivity,
                null,
                sectionHeaderModel,
                tabId,
                /* actionDelegate= */ null,
                mOptionsCoordinator,
                uiConfig,
                mProfileMock);
    }
}
