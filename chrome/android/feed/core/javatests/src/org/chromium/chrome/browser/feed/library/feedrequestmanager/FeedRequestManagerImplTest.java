// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Base64;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo.FeatureName;
import org.chromium.chrome.browser.feed.library.api.internal.common.DismissActionWithSemanticProperties;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.locale.LocaleUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.testing.RequiredConsumer;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.actionmanager.FakeActionReader;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.chrome.browser.feed.library.testing.host.stream.FakeTooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.testing.network.FakeNetworkClient;
import org.chromium.chrome.browser.feed.library.testing.protocoladapter.FakeProtocolAdapter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ActionTypeProto.ActionType;
import org.chromium.components.feed.core.proto.wire.CapabilityProto.Capability;
import org.chromium.components.feed.core.proto.wire.ClientInfoProto.ClientInfo;
import org.chromium.components.feed.core.proto.wire.ClientInfoProto.ClientInfo.AppType;
import org.chromium.components.feed.core.proto.wire.ClientInfoProto.ClientInfo.PlatformType;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DisplayInfoProto.DisplayInfo;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.Action;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.FeedActionQueryData;
import org.chromium.components.feed.core.proto.wire.FeedActionQueryDataProto.FeedActionQueryDataItem;
import org.chromium.components.feed.core.proto.wire.FeedQueryProto.FeedQuery;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponse;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request.RequestVersion;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.SemanticPropertiesProto.SemanticProperties;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version.Architecture;
import org.chromium.components.feed.core.proto.wire.VersionProto.Version.BuildType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Test of the {@link FeedRequestManagerImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
@Features.
DisableFeatures({ChromeFeatureList.REPORT_FEED_USER_ACTIONS, ChromeFeatureList.INTEREST_FEED_V2,
        ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
public class FeedRequestManagerImplTest {
    private static final int NOT_FOUND = 404;
    private static final String TABLE = "table";
    private static final String TABLE_2 = "table2";
    private static final String CONTENT_DOMAIN = "contentDomain";
    private static final String CONTENT_DOMAIN_2 = "contentDomain2";
    public static final long ID = 42;
    private static final long ID_2 = 2;
    private static final String APP_VERSION_STRING = "5.7";

    private final FakeClock mFakeClock = new FakeClock();
    private final TimingUtils mTimingUtils = new TimingUtils();
    private final Configuration mConfiguration = new Configuration.Builder().build();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private SchedulerApi mScheduler;
    @Mock
    private ApplicationInfo mApplicationInfo;
    @Mock
    private IdentityServicesProvider.Natives mIdentityServicesProviderJniMock;
    @Mock
    private Profile mProfileMock;
    @Mock
    private IdentityManager mIdentifiyManagerMock;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;

    private Context mContext;
    private ExtensionRegistryLite mRegistry;
    private FeedRequestManagerImpl mRequestManager;
    private FakeActionReader mFakeActionReader;
    private FakeMainThreadRunner mFakeMainThreadRunner;
    private FakeProtocolAdapter mFakeProtocolAdapter;
    private FakeThreadUtils mFakeThreadUtils;
    private FakeTaskQueue mFakeTaskQueue;
    private FakeBasicLoggingApi mFakeBasicLoggingApi;
    private FakeNetworkClient mFakeNetworkClient;
    private FakeTooltipSupportedApi mFakeTooltipSupportedApi;
    private RequiredConsumer<Result<Model>> mConsumer;
    private Result<Model> mConsumedResult = Result.failure();
    private HttpResponse mFailingResponse;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.getResources().getConfiguration().locale = Locale.US;
        FeedExtensionRegistry feedExtensionRegistry = new FeedExtensionRegistry(ArrayList::new);
        mRegistry = ExtensionRegistryLite.newInstance();
        mRegistry.add(FeedRequest.feedRequest);
        mFakeActionReader = new FakeActionReader();
        mFakeProtocolAdapter = new FakeProtocolAdapter();
        mFakeBasicLoggingApi = new FakeBasicLoggingApi();
        mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
        mFakeMainThreadRunner =
                FakeMainThreadRunner.runTasksImmediatelyWithThreadChecks(mFakeThreadUtils);
        mFakeNetworkClient = new FakeNetworkClient(mFakeThreadUtils);
        mFakeNetworkClient.setDefaultResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeTaskQueue.initialize(() -> {});
        mFakeTooltipSupportedApi = new FakeTooltipSupportedApi(mFakeThreadUtils);
        mFailingResponse =
                createHttpResponse(/* responseCode= */ NOT_FOUND, Response.getDefaultInstance());
        mConsumer = new RequiredConsumer<>(input -> { mConsumedResult = input; });
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.KITKAT);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "4.4.3");
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "dev-keys");
        when(mApplicationInfo.getAppType()).thenReturn(ApplicationInfo.AppType.CHROME);
        when(mApplicationInfo.getArchitecture()).thenReturn(ApplicationInfo.Architecture.ARM);
        when(mApplicationInfo.getBuildType()).thenReturn(ApplicationInfo.BuildType.DEV);
        when(mApplicationInfo.getVersionString()).thenReturn(APP_VERSION_STRING);

        Profile.setLastUsedProfileForTesting(mProfileMock);
        jniMocker.mock(IdentityServicesProviderJni.TEST_HOOKS, mIdentityServicesProviderJniMock);
        when(mIdentityServicesProviderJniMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentifiyManagerMock);

        jniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefService);

        mRequestManager = new FeedRequestManagerImpl(mConfiguration, mFakeNetworkClient,
                mFakeProtocolAdapter, feedExtensionRegistry, mScheduler, mFakeTaskQueue,
                mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext, mApplicationInfo,
                mFakeMainThreadRunner, mFakeBasicLoggingApi, mFakeTooltipSupportedApi);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testTriggerRefresh() throws Exception {
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        assertThat(mFakeTooltipSupportedApi.getLatestFeatureName()).isNull();

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.PRIORITY_PARAM))
                .isEqualTo(RequestHelper.PRIORITY_VALUE_BACKGROUND);

        Request request = getRequestFromHttpRequest(httpRequest);
        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testTriggerRefresh_setNoticeCardPrefAndRecordBothHistograms() throws Exception {
        MetricsUtils.HistogramDelta obsoleteNoticeCardNotFulfilledDelta =
                new MetricsUtils.HistogramDelta(
                        "ContentSuggestions.Feed.NoticeCardFulfilled", 0 /*false*/);
        MetricsUtils.HistogramDelta obsoleteNoticeCardFulfilledDelta =
                new MetricsUtils.HistogramDelta(
                        "ContentSuggestions.Feed.NoticeCardFulfilled", 1 /*true*/);
        MetricsUtils.HistogramDelta noticeCardNotFulfilledDelta = new MetricsUtils.HistogramDelta(
                "ContentSuggestions.Feed.NoticeCardFulfilled2", 0 /*false*/);
        MetricsUtils.HistogramDelta noticeCardFulfilledDelta = new MetricsUtils.HistogramDelta(
                "ContentSuggestions.Feed.NoticeCardFulfilled2", 1 /*true*/);

        // Skip the read of the int that determines the length of the encoded proto. This is to
        // avoid having to encode the length which is a feature we don't want to test here.
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, false)
                        .build();

        mRequestManager = new FeedRequestManagerImpl(configuration, mFakeNetworkClient,
                mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), mScheduler,
                mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext,
                mApplicationInfo, mFakeMainThreadRunner, mFakeBasicLoggingApi,
                mFakeTooltipSupportedApi);

        // Trigger a refresh that has a notice card.
        Response response =
                Response.newBuilder()
                        .setExtension(FeedResponse.feedResponse,
                                FeedResponse.newBuilder()
                                        .addServerCapabilities(
                                                Capability.REPORT_FEED_USER_ACTIONS_NOTICE_CARD)
                                        .build())
                        .build();
        mFakeNetworkClient.addResponse(new HttpResponse(200, response.toByteArray(), false));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});
        verify(mPrefService, times(1)).setBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD, true);
        assertThat(noticeCardNotFulfilledDelta.getDelta()).isEqualTo(0);
        assertThat(noticeCardFulfilledDelta.getDelta()).isEqualTo(1);

        // Trigger a refresh that doesn't have a notice card.
        mFakeNetworkClient.addResponse(
                new HttpResponse(200, Response.getDefaultInstance().toByteArray(), false));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});
        assertThat(obsoleteNoticeCardNotFulfilledDelta.getDelta()).isEqualTo(1);
        assertThat(obsoleteNoticeCardFulfilledDelta.getDelta()).isEqualTo(1);
        assertThat(noticeCardNotFulfilledDelta.getDelta()).isEqualTo(1);
        assertThat(noticeCardFulfilledDelta.getDelta()).isEqualTo(1);
    }

    @Test
    public void testTriggerRefresh_setLastRefreshWasSignedInPref() throws Exception {
        // Skip the read of the int that determines the length of the encoded proto. This is to
        // avoid having to encode the length which is a feature we don't want to test here.
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, false)
                        .build();

        mRequestManager = new FeedRequestManagerImpl(configuration, mFakeNetworkClient,
                mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), mScheduler,
                mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext,
                mApplicationInfo, mFakeMainThreadRunner, mFakeBasicLoggingApi,
                mFakeTooltipSupportedApi);

        mFakeNetworkClient.addResponse(
                new HttpResponse(200, Response.getDefaultInstance().toByteArray(), true));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        verify(mPrefService, times(1)).setBoolean(Pref.LAST_REFRESH_WAS_SIGNED_IN, true);
    }

    @Test
    public void testLoadMore_dontSetNoticeCardPrefAndOnlyRecordObsoleteHistogram()
            throws Exception {
        MetricsUtils.HistogramDelta obsoleteNoticeCardNotFulfilledDelta =
                new MetricsUtils.HistogramDelta(
                        "ContentSuggestions.Feed.NoticeCardFulfilled", 0 /*false*/);
        MetricsUtils.HistogramDelta obsoleteNoticeCardFulfilledDelta =
                new MetricsUtils.HistogramDelta(
                        "ContentSuggestions.Feed.NoticeCardFulfilled", 1 /*true*/);
        MetricsUtils.HistogramDelta noticeCardNotFulfilledDelta = new MetricsUtils.HistogramDelta(
                "ContentSuggestions.Feed.NoticeCardFulfilled2", 0 /*false*/);
        MetricsUtils.HistogramDelta noticeCardFulfilledDelta = new MetricsUtils.HistogramDelta(
                "ContentSuggestions.Feed.NoticeCardFulfilled2", 1 /*true*/);

        // Skip the read of the int that determines the length of the encoded proto. This is to
        // avoid having to encode the length which is a feature we don't want to test here.
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, false)
                        .build();

        mRequestManager = new FeedRequestManagerImpl(configuration, mFakeNetworkClient,
                mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), mScheduler,
                mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext,
                mApplicationInfo, mFakeMainThreadRunner, mFakeBasicLoggingApi,
                mFakeTooltipSupportedApi);

        mFakeThreadUtils.enforceMainThread(false);

        // Trigger a load more with a notice card in the query response.
        Response response =
                Response.newBuilder()
                        .setExtension(FeedResponse.feedResponse,
                                FeedResponse.newBuilder()
                                        .addServerCapabilities(
                                                Capability.REPORT_FEED_USER_ACTIONS_NOTICE_CARD)
                                        .build())
                        .build();
        mFakeNetworkClient.addResponse(new HttpResponse(200, response.toByteArray(), false));
        StreamToken token =
                StreamToken.newBuilder()
                        .setNextPageToken(ByteString.copyFrom("abc", Charset.defaultCharset()))
                        .build();
        mRequestManager.loadMore(token, ConsistencyToken.getDefaultInstance(), input -> {});

        // Trigger a load more without a notice card in the query response.
        mFakeNetworkClient.addResponse(
                new HttpResponse(200, Response.getDefaultInstance().toByteArray(), false));
        mRequestManager.loadMore(token, ConsistencyToken.getDefaultInstance(), input -> {});

        // Verify that only the obsolete histograms were recorded.
        assertThat(noticeCardNotFulfilledDelta.getDelta()).isEqualTo(0);
        assertThat(noticeCardFulfilledDelta.getDelta()).isEqualTo(0);
        assertThat(obsoleteNoticeCardNotFulfilledDelta.getDelta()).isEqualTo(1);
        assertThat(obsoleteNoticeCardFulfilledDelta.getDelta()).isEqualTo(1);
        // Verify that no attempts were made to update the notice card presence pref.
        verify(mPrefService, never()).setBoolean(eq(Pref.LAST_FETCH_HAD_NOTICE_CARD), anyBoolean());
    }

    @Test
    public void testLoadMore_dontSetLastRefreshWasSignedInPref() throws Exception {
        // Skip the read of the int that determines the length of the encoded proto. This is to
        // avoid having to encode the length which is a feature we don't want to test here.
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, false)
                        .build();

        mRequestManager = new FeedRequestManagerImpl(configuration, mFakeNetworkClient,
                mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), mScheduler,
                mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext,
                mApplicationInfo, mFakeMainThreadRunner, mFakeBasicLoggingApi,
                mFakeTooltipSupportedApi);

        mFakeNetworkClient.addResponse(
                new HttpResponse(200, Response.getDefaultInstance().toByteArray(), false));
        StreamToken token =
                StreamToken.newBuilder()
                        .setNextPageToken(ByteString.copyFrom("abc", Charset.defaultCharset()))
                        .build();
        mFakeThreadUtils.enforceMainThread(false);
        mRequestManager.loadMore(token, ConsistencyToken.getDefaultInstance(), input -> {});

        verify(mPrefService, never()).setBoolean(Pref.LAST_REFRESH_WAS_SIGNED_IN, true);
    }

    @Test
    public void testTriggerRefresh_FeedUiCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.FEED_UI_ENABLED, Capability.FEED_UI);
    }

    @Test
    public void testTriggerRefresh_undoableActionCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.UNDOABLE_ACTIONS_ENABLED, Capability.UNDOABLE_ACTIONS);
    }

    @Test
    public void testTriggerRefresh_manageInterestsCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.MANAGE_INTERESTS_ENABLED, Capability.MANAGE_INTERESTS);
    }

    @Test
    public void testTriggerRefresh_sendFeedbackCapabilityAdded() throws Exception {
        testCapabilityAdded(Capability.SEND_FEEDBACK);
    }

    @Test
    public void testTriggerRefresh_tooltipCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, Capability.CARD_MENU_TOOLTIP);
    }

    @Test
    public void testTriggerRefresh_tooltipCapabilityNotAdded() throws Exception {
        // If the config key for card menu tool tip is set but the
        // TooltipSupportedApi.wouldTriggerHelpUi() returns false, then the capability should not be
        // added and only the BASE_UI should be present.
        mFakeTooltipSupportedApi.addUnsupportedFeature(FeatureName.CARD_MENU_TOOLTIP);
        testCapabilityAdded(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE /* capability= empty */);
    }

    @Test
    public void testTriggerRefresh_useSecondaryPageRequestAdded() throws Exception {
        testCapabilityAdded(
                ConfigKey.USE_SECONDARY_PAGE_REQUEST, Capability.USE_SECONDARY_PAGE_REQUEST);
    }

    @Test
    public void testTriggerRefresh_articleSnippetsAdded() throws Exception {
        testCapabilityAdded(ConfigKey.SNIPPETS_ENABLED, Capability.ARTICLE_SNIPPETS);
    }

    @Test
    public void testTriggerRefresh_enableCarouselsAdded() throws Exception {
        testCapabilityAdded(ConfigKey.ENABLE_CAROUSELS, Capability.CAROUSELS);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testTriggerRefresh_enableFeedActions() throws Exception {
        testCapabilityAdded(Capability.CLICK_ACTION, Capability.VIEW_ACTION,
                Capability.REPORT_FEED_USER_ACTIONS_NOTICE_CARD);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void testTriggerRefresh_acknowledgeNoticeCard_whenClicksThresholdReached()
            throws Exception {
        // Simulate enough clicks.
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(1);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(0);

        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        assertTrue(getRequestFromHttpRequest(httpRequest)
                           .getExtension(FeedRequest.feedRequest)
                           .getFeedQuery()
                           .getChromeFulfillmentInfo()
                           .getNoticeCardAcknowledged());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void testTriggerRefresh_acknowledgeNoticeCard_whenViewsThresholdReached()
            throws Exception {
        // Simulate enough views.
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(0);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(3);

        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        assertTrue(getRequestFromHttpRequest(httpRequest)
                           .getExtension(FeedRequest.feedRequest)
                           .getFeedQuery()
                           .getChromeFulfillmentInfo()
                           .getNoticeCardAcknowledged());
    }

    @Test
    public void testTriggerRefresh_dontAcknowledgeNoticeCard_whenFeatureDisabled()
            throws Exception {
        // Simulate enough views and clicks.
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(1);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(3);

        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        assertFalse(getRequestFromHttpRequest(httpRequest)
                            .getExtension(FeedRequest.feedRequest)
                            .getFeedQuery()
                            .getChromeFulfillmentInfo()
                            .getNoticeCardAcknowledged());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
    public void testTriggerRefresh_dontAcknowledgeNoticeCard_whenCountThresholdsNotReached()
            throws Exception {
        // Simulate not enough views nor clicks.
        when(mPrefService.getInteger(Pref.NOTICE_CARD_CLICKS_COUNT)).thenReturn(0);
        when(mPrefService.getInteger(Pref.NOTICE_CARD_VIEWS_COUNT)).thenReturn(2);

        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        assertFalse(getRequestFromHttpRequest(httpRequest)
                            .getExtension(FeedRequest.feedRequest)
                            .getFeedQuery()
                            .getChromeFulfillmentInfo()
                            .getNoticeCardAcknowledged());
    }

    @Test
    public void testActionData_simpleDismiss() throws Exception {
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Collections.singletonList(ID);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Collections.singletonList(FeedActionQueryDataItem.newBuilder()
                                                  .setIdIndex(0)
                                                  .setContentDomainIndex(0)
                                                  .setTableIndex(0)
                                                  .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_uniqueDismisses() throws Exception {
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, null));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_overlappingDismisses() throws Exception {
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN, TABLE, null));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(0)
                                .setTableIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_simpleDismissWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Collections.singletonList(ID);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Collections.singletonList(FeedActionQueryDataItem.newBuilder()
                                                  .setIdIndex(0)
                                                  .setContentDomainIndex(0)
                                                  .setTableIndex(0)
                                                  .setSemanticPropertiesIndex(0)
                                                  .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_uniqueDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        byte[] semanticPropertiesBytes2 = {7, 43, 91};
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, semanticPropertiesBytes2));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Arrays.asList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build(),
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes2))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .setSemanticPropertiesIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(1)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_overlappingDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes),
                buildDismissAction(ID_2, CONTENT_DOMAIN, TABLE_2, semanticPropertiesBytes));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .setSemanticPropertiesIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(0)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_someDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        mFakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, semanticPropertiesBytes));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testHandleResponse() throws Exception {
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, mConsumer);

        assertThat(mFakeProtocolAdapter.getLastResponse()).isEqualTo(Response.getDefaultInstance());
        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mConsumedResult.isSuccessful()).isTrue();
    }

    @Test
    public void testHandleResponse_notFound() throws Exception {
        mFakeNetworkClient.addResponse(mFailingResponse);
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, mConsumer);

        verify(mScheduler).onRequestError(NOT_FOUND);
        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mConsumedResult.isSuccessful()).isFalse();
    }

    @Test
    public void testHandleResponse_pageNotFound() throws Exception {
        mFakeNetworkClient.addResponse(mFailingResponse);
        StreamToken token =
                StreamToken.newBuilder()
                        .setNextPageToken(ByteString.copyFrom("abc", Charset.defaultCharset()))
                        .build();
        mFakeThreadUtils.enforceMainThread(false);
        mRequestManager.loadMore(token, ConsistencyToken.getDefaultInstance(), mConsumer);

        verify(mScheduler, never()).onRequestError(NOT_FOUND);
        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mConsumedResult.isSuccessful()).isFalse();
    }

    @Test
    public void testHandleResponse_missingLengthPrefixNotSupported() {
        mFakeNetworkClient.addResponse(new HttpResponse(
                /* responseCode= */ 200, Response.getDefaultInstance().toByteArray(), false));
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, mConsumer);
        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mConsumedResult.isSuccessful()).isFalse();
        assertThat(mFakeProtocolAdapter.getLastResponse()).isNull();
    }

    @Test
    public void testGetWireRequestResponse_unknown() throws Exception {
        testReason(RequestReason.UNKNOWN, FeedQuery.RequestReason.UNKNOWN_REQUEST_REASON,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    public void testGetWireRequestResponse_zeroState() throws Exception {
        testReason(RequestReason.ZERO_STATE, FeedQuery.RequestReason.ZERO_STATE_REFRESH,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    public void testGetWireRequestResponse_hostRequested() throws Exception {
        testReason(RequestReason.HOST_REQUESTED, FeedQuery.RequestReason.SCHEDULED_REFRESH,
                RequestHelper.PRIORITY_VALUE_BACKGROUND);
    }

    @Test
    public void testGetWireRequestResponse_openWithContent() throws Exception {
        testReason(RequestReason.OPEN_WITH_CONTENT, FeedQuery.RequestReason.WITH_CONTENT,
                RequestHelper.PRIORITY_VALUE_BACKGROUND);
    }

    @Test
    public void testGetWireRequestResponse_manualContinuation() throws Exception {
        testReason(RequestReason.MANUAL_CONTINUATION, FeedQuery.RequestReason.NEXT_PAGE_SCROLL,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    public void testGetWireRequestResponse_automaticContinuation() throws Exception {
        testReason(RequestReason.AUTOMATIC_CONTINUATION, FeedQuery.RequestReason.NEXT_PAGE_SCROLL,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    public void testGetWireRequestResponse_openWithoutContent() throws Exception {
        testReason(RequestReason.OPEN_WITHOUT_CONTENT, FeedQuery.RequestReason.INITIAL_LOAD,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    public void testGetWireRequestResponse_clearAll() throws Exception {
        testReason(RequestReason.CLEAR_ALL, FeedQuery.RequestReason.CLEAR_ALL,
                RequestHelper.PRIORITY_VALUE_INTERACTIVE);
    }

    @Test
    @Config(qualifiers = "en-rGB", sdk = VERSION_CODES.LOLLIPOP)
    public void testClientInfo_postLollipop() throws Exception {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.LOLLIPOP);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "7.1.2b4.1");
        ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", new String[] {"arm64-v8a"});
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "release-keys");
        when(mApplicationInfo.getAppType()).thenReturn(ApplicationInfo.AppType.SEARCH_APP);
        when(mApplicationInfo.getArchitecture()).thenReturn(ApplicationInfo.Architecture.ARM64);
        when(mApplicationInfo.getBuildType()).thenReturn(ApplicationInfo.BuildType.RELEASE);
        when(mApplicationInfo.getVersionString()).thenReturn("1.2.3.4");

        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();

        Request request = getRequestFromHttpRequest(httpRequest);
        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .setClientInfo(
                                                ClientInfo.newBuilder()
                                                        .setPlatformType(PlatformType.ANDROID_ID)
                                                        .setPlatformVersion(
                                                                Version.newBuilder()
                                                                        .setMajor(7)
                                                                        .setMinor(1)
                                                                        .setBuild(2)
                                                                        .setRevision(1)
                                                                        .setArchitecture(
                                                                                Architecture.ARM64)
                                                                        .setBuildType(
                                                                                BuildType.RELEASE)
                                                                        .setApiVersion(
                                                                                VERSION_CODES
                                                                                        .LOLLIPOP)
                                                                        .build())
                                                        .setLocale(LocaleUtils.getLanguageTag(
                                                                mContext))
                                                        .setAppType(AppType.GSA)
                                                        .setAppVersion(
                                                                Version.newBuilder()
                                                                        .setMajor(1)
                                                                        .setMinor(2)
                                                                        .setBuild(3)
                                                                        .setRevision(4)
                                                                        .setArchitecture(
                                                                                Architecture.ARM64)
                                                                        .setBuildType(
                                                                                BuildType.RELEASE)
                                                                        .build())
                                                        .addDisplayInfo(
                                                                DisplayInfo.newBuilder()
                                                                        .setScreenDensity(1.0f)
                                                                        .setScreenWidthInPixels(320)
                                                                        .setScreenHeightInPixels(
                                                                                470))
                                                        .build())
                                        .addClientCapability(Capability.SEND_FEEDBACK)
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    private void testReason(@RequestReason int reason, FeedQuery.RequestReason expectedReason,
            String expectedPriority) throws Exception {
        mFakeNetworkClient.addResponse(mFailingResponse);
        mRequestManager.triggerRefresh(reason, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        Request request = getRequestFromHttpRequest(httpRequest);
        assertThat(request.getExtension(FeedRequest.feedRequest).getFeedQuery().getReason())
                .isEqualTo(expectedReason);
        assertThat(mFakeBasicLoggingApi.serverRequestReason).isEqualTo(reason);
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.PRIORITY_PARAM))
                .isEqualTo(expectedPriority);
    }

    private static void assertHttpRequestFormattedCorrectly(
            HttpRequest httpRequest, Context context) {
        assertThat(httpRequest.getBody()).hasLength(0);
        assertThat(httpRequest.getMethod()).isEqualTo(HttpMethod.GET);
        assertThat(httpRequest.getUri().getQueryParameter("fmt")).isEqualTo("bin");
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD))
                .isNotNull();
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.LOCALE_PARAM))
                .isEqualTo(LocaleUtils.getLanguageTag(context));
    }

    private static HttpResponse createHttpResponse(int responseCode, Response response)
            throws IOException {
        byte[] rawResponse = response.toByteArray();
        ByteBuffer buffer = ByteBuffer.allocate(rawResponse.length + (Integer.SIZE / 8));
        CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(buffer);
        codedOutputStream.writeUInt32NoTag(rawResponse.length);
        codedOutputStream.writeRawBytes(rawResponse);
        codedOutputStream.flush();
        return new HttpResponse(responseCode, buffer.array(), false);
    }

    private static DismissActionWithSemanticProperties buildDismissAction(
            long id, String contentDomain, String table, byte /*@Nullable*/[] semanticProperties) {
        ContentId contentId = ContentId.newBuilder()
                                      .setTable(table)
                                      .setContentDomain(contentDomain)
                                      .setId(id)
                                      .build();
        return new DismissActionWithSemanticProperties(contentId, semanticProperties);
    }

    private Request getRequestFromHttpRequest(HttpRequest httpRequest) throws Exception {
        return Request.parseFrom(Base64.decode(httpRequest.getUri().getQueryParameter(
                                                       RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                                         Base64.URL_SAFE),
                mRegistry);
    }

    private static FeedRequest.Builder getTestFeedRequestBuilder() {
        return FeedRequest.newBuilder()
                .setConsistencyToken(ConsistencyToken.getDefaultInstance())
                .setClientInfo(
                        ClientInfo.newBuilder()
                                .setPlatformType(PlatformType.ANDROID_ID)
                                .setPlatformVersion(Version.newBuilder()
                                                            .setMajor(4)
                                                            .setMinor(4)
                                                            .setBuild(3)
                                                            .setArchitecture(Architecture.ARM)
                                                            .setBuildType(BuildType.DEV)
                                                            .setApiVersion(VERSION_CODES.KITKAT)
                                                            .build())
                                .setLocale(Locale.US.toLanguageTag())
                                .setAppType(AppType.CHROME)
                                .setAppVersion(Version.newBuilder()
                                                       .setMajor(5)
                                                       .setMinor(7)
                                                       .setArchitecture(Architecture.ARM)
                                                       .setBuildType(BuildType.DEV)
                                                       .build())
                                .addDisplayInfo(DisplayInfo.newBuilder()
                                                        .setScreenDensity(1.0f)
                                                        .setScreenWidthInPixels(320)
                                                        .setScreenHeightInPixels(470))
                                .build());
    }

    private void testCapabilityAdded(String configKey, Capability... capability) throws Exception {
        Configuration configuration = new Configuration.Builder().put(configKey, true).build();
        testCapabilityAddedWithConfig(configuration, capability);
    }

    private void testCapabilityAdded(Capability... capability) throws Exception {
        Configuration configuration = new Configuration.Builder().build();
        testCapabilityAddedWithConfig(configuration, capability);
    }

    private void testCapabilityAddedWithConfig(
            Configuration configuration, Capability... capability) throws Exception {
        mRequestManager = new FeedRequestManagerImpl(configuration, mFakeNetworkClient,
                mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), mScheduler,
                mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mFakeActionReader, mContext,
                mApplicationInfo, mFakeMainThreadRunner, mFakeBasicLoggingApi,
                mFakeTooltipSupportedApi);
        mRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, mContext);

        Set<Capability> expectedCap = EnumSet.of(Capability.BASE_UI, Capability.SEND_FEEDBACK);
        Collections.addAll(expectedCap, capability);

        Request request = getRequestFromHttpRequest(httpRequest);
        assertThat(request.getExtension(FeedRequest.feedRequest).getClientCapabilityList())
                .containsExactlyElementsIn(expectedCap);
    }
}
