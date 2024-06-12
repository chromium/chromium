// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.BottomSheetOpenedSource;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator.OmniboxIconController;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Tests for {@link MerchantTrustSignalsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class MerchantTrustSignalsCoordinatorTest {

    @Mock private Context mMockContext;

    @Mock private Resources mMockResources;

    @Mock private MerchantTrustMessageScheduler mMockMerchantMessageScheduler;

    @Mock private ObservableSupplier<Tab> mMockTabProvider;

    @Mock private Tab mMockTab;

    @Mock private ObservableSupplier<Profile> mMockProfileSupplier;

    @Mock private Profile mMockProfile;

    @Mock private MerchantTrustMetrics mMockMetrics;

    @Mock private WebContents mMockWebContents;

    @Mock private GURL mMockGurl;

    @Mock private GURL mMockGurl2;

    @Mock private MerchantTrustSignalsDataProvider mMockMerchantTrustDataProvider;

    @Mock private MerchantTrustSignalsEventStorage mMockMerchantTrustStorage;

    @Mock private MerchantTrustSignalsStorageFactory mMockMerchantTrustStorageFactory;

    @Mock private MerchantTrustSignalsEvent mMockMerchantTrustSignalsEvent;

    @Mock private MerchantTrustBottomSheetCoordinator mMockDetailsTabCoordinator;

    @Mock private NavigationHandle mMockNavigationHandle;

    @Mock private NavigationHandle mMockNavigationHandle2;

    @Mock private PrefService mMockPrefService;

    @Mock private WindowAndroid mMockWindowAndroid;

    @Mock private OmniboxIconController mMockIconController;

    @Mock private Drawable mMockDrawable;

    @Mock private Tracker mMockTracker;

    @Captor private ArgumentCaptor<Runnable> mOnBottomSheetDismissedCaptor;

    private static final String FAKE_HOST = "fake_host";
    private static final String DIFFERENT_HOST = "different_host";
    private static final String FAKE_URL = "fake_url";

    private MerchantInfo mDummyMerchantTrustSignals =
            new MerchantInfo(4.5f, 100, null, false, 0f, false, false);
    private MerchantTrustSignalsCoordinator mCoordinator;
    private FeatureList.TestValues mTestValues;
    private String mSerializedTimestamps;
    private MerchantTrustMessageContext mMessageContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockResources).when(mMockContext).getResources();
        doReturn("").when(mMockResources).getString(anyInt());
        doReturn("").when(mMockResources).getQuantityString(anyInt(), anyInt(), any());
        doReturn(FAKE_HOST).when(mMockGurl).getHost();
        doReturn(FAKE_HOST).when(mMockGurl).getSpec();
        doReturn(mMockMerchantTrustStorage)
                .when(mMockMerchantTrustStorageFactory)
                .getForLastUsedProfile();
        doReturn(mMockGurl).when(mMockNavigationHandle).getUrl();
        doReturn(mMockGurl2).when(mMockNavigationHandle2).getUrl();
        doReturn(true).when(mMockNavigationHandle).isInPrimaryMainFrame();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn(FAKE_HOST).when(mMockMerchantTrustSignalsEvent).getKey();
        doReturn(mMockProfile).when(mMockProfileSupplier).get();
        doReturn(false).when(mMockProfile).isOffTheRecord();
        doReturn(FAKE_HOST).when(mMockGurl).getSpec();
        doReturn(true).when(mMockTabProvider).hasValue();
        doReturn(mMockTab).when(mMockTabProvider).get();
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doAnswer((Answer<String>) invocation -> mSerializedTimestamps)
                .when(mMockPrefService)
                .getString(eq(Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME));
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    mSerializedTimestamps = (String) invocation.getArguments()[1];
                                    return null;
                                })
                .when(mMockPrefService)
                .setString(
                        eq(Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME), any(String.class));

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData(FAKE_HOST, mMockMerchantTrustSignalsEvent);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM,
                "-1");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_DISABLED_PARAM,
                "false");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_RATING_THRESHOLD_PARAM,
                "4.0");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig
                        .TRUST_SIGNALS_NON_PERSONALIZED_FAMILIARITY_SCORE_THRESHOLD_PARAM,
                "0.8");
        FeatureList.setTestValues(mTestValues);

        mMessageContext = new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents);
        mCoordinator =
                spy(
                        new MerchantTrustSignalsCoordinator(
                                mMockContext,
                                mMockWindowAndroid,
                                mMockMerchantMessageScheduler,
                                mMockTabProvider,
                                mMockMerchantTrustDataProvider,
                                mMockProfileSupplier,
                                mMockMetrics,
                                mMockDetailsTabCoordinator,
                                mMockMerchantTrustStorageFactory));
        doReturn(0.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));
        doReturn(mMockPrefService).when(mCoordinator).getPrefService();
        doReturn(mMockDrawable).when(mCoordinator).getStoreIconDrawable();
        doReturn(true).when(mCoordinator).isOnSecureWebsite(any(WebContents.class));
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        verify(mMockMerchantTrustStorageFactory, times(1)).destroy();
    }

    @SmallTest
    @Test
    public void testFetchTrustSiganl_WithoutScheduledMessage() {
        setMockTrustSignalsData(null);

        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        mCoordinator.onFinishEligibleNavigation(mMessageContext);
        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForUrl(eq(mMockProfile), eq(mMockGurl), any(Callback.class));
    }

    @SmallTest
    @Test
    public void testFetchTrustSiganl_WithScheduledMessage() {
        setMockTrustSignalsData(null);

        doReturn(FAKE_HOST).when(mMockGurl2).getHost();
        doReturn(DIFFERENT_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();
        mCoordinator.onFinishEligibleNavigation(mMessageContext);
        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_SAME_DOMAIN));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForUrl(eq(mMockProfile), eq(mMockGurl), any(Callback.class));
    }

    @SmallTest
    @Test
    public void testFetchTrustSiganl_WithScheduledMessage_ForSameUrl() {
        setMockTrustSignalsData(null);

        doReturn(FAKE_HOST).when(mMockGurl2).getHost();
        doReturn(FAKE_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();
        mCoordinator.onFinishEligibleNavigation(mMessageContext);
        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForUrl(eq(mMockProfile), eq(mMockGurl), any(Callback.class));
    }

    @SmallTest
    @Test
    public void testFetchTrustSiganl_WithScheduledMessage_ForDifferentHost() {
        setMockTrustSignalsData(null);

        doReturn(DIFFERENT_HOST).when(mMockGurl2).getHost();
        doReturn(DIFFERENT_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();
        mCoordinator.onFinishEligibleNavigation(mMessageContext);
        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForUrl(eq(mMockProfile), eq(mMockGurl), any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_ShouldNotExpediteMessage() {
        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMetrics, times(1)).recordUkmOnDataAvailable(eq(mMockWebContents));
        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(true, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_ShouldExpediteMessage() {
        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, true);

        verify(mMockMetrics, times(1)).recordUkmOnDataAvailable(eq(mMockWebContents));
        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(true, true);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_LastEventWithinTimeWindow() {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM,
                "60000");
        doReturn(System.currentTimeMillis() - TimeUnit.SECONDS.toMillis(10))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_NoPreviousEvent() {
        setMockTrustSignalsEventData(FAKE_HOST, null);

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(true, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_NoMerchantTrustData() {
        mCoordinator.maybeDisplayMessage(null, mMessageContext, false);

        verify(mMockMetrics, times(0)).recordUkmOnDataAvailable(eq(mMockWebContents));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithInvalidStorage() {
        doReturn(null).when(mMockMerchantTrustStorageFactory).getForLastUsedProfile();

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithInvalidNavigationHandler() {
        doReturn(null).when(mMockNavigationHandle).getUrl();

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithSiteEngagementAboveThreshold() {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM,
                "true");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM,
                "80.0");
        doReturn(90.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithSiteEngagementBelowThreshold() {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM,
                "true");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM,
                "80.0");
        doReturn(70.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(true, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_AlreadyReachedMaxAllowedNumber() {
        doReturn(true).when(mCoordinator).hasReachedMaxAllowedMessageNumberInGivenTime();

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_OnNonSecureWebsite() {
        doReturn(false).when(mCoordinator).isOnSecureWebsite(any(WebContents.class));

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_MessageDisabledForAllMerchants() {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_DISABLED_PARAM,
                "true");

        mCoordinator.maybeDisplayMessage(mDummyMerchantTrustSignals, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_MessageDisabledForThisMerchant() {
        MerchantInfo merchantInfo = new MerchantInfo(4.5f, 100, null, false, 0f, false, true);

        mCoordinator.maybeDisplayMessage(merchantInfo, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_MerchantRatingBelowThreshold() {
        MerchantInfo merchantInfo = new MerchantInfo(3.5f, 100, null, false, 0f, false, false);

        mCoordinator.maybeDisplayMessage(merchantInfo, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_NonPersonalizedFamiliarityScoreAboveThreshold() {
        MerchantInfo merchantInfo = new MerchantInfo(4.5f, 100, null, false, 0.9f, false, false);

        mCoordinator.maybeDisplayMessage(merchantInfo, mMessageContext, false);

        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verifySchedulingMessage(false, false);
    }

    @SmallTest
    @Test
    public void testOnMessageEnqueued() {
        mCoordinator.onMessageEnqueued(null);
        verify(mMockMerchantTrustStorage, times(0)).save(any(MerchantTrustSignalsEvent.class));

        mCoordinator.onMessageEnqueued(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));
        verify(mMockMetrics, times(1)).recordUkmOnMessageSeen(eq(mMockWebContents));
        verify(mCoordinator, times(1)).updateShownMessagesTimestamp();
        verify(mMockMerchantTrustStorage, times(1)).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    public void testOnMessageDismissed_Timer() {
        mCoordinator.onMessageDismissed(DismissReason.TIMER, FAKE_URL);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.TIMER));
        verify(mCoordinator, times(1)).maybeShowStoreIcon(eq(FAKE_URL), eq(true));
    }

    @SmallTest
    @Test
    public void testOnMessageDismissed_Gesture() {
        mCoordinator.onMessageDismissed(DismissReason.GESTURE, FAKE_URL);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.GESTURE));
        verify(mCoordinator, times(1)).maybeShowStoreIcon(eq(FAKE_URL), eq(false));
    }

    @SmallTest
    @Test
    public void testOnMessagePrimaryAction() {
        mCoordinator.onMessagePrimaryAction(mDummyMerchantTrustSignals, FAKE_URL);
        verify(mMockMetrics, times(1)).recordMetricsForMessageTapped();
        verify(mMockMetrics, times(1)).recordUkmOnMessageClicked(eq(mMockWebContents));
        verify(mMockMetrics, times(1))
                .recordMetricsForBottomSheetOpenedSource(eq(BottomSheetOpenedSource.FROM_MESSAGE));
        verify(mMockDetailsTabCoordinator, times(1))
                .requestOpenSheet(
                        any(), any(String.class), mOnBottomSheetDismissedCaptor.capture());
        mOnBottomSheetDismissedCaptor.getValue().run();
        verify(mCoordinator, times(1)).maybeShowStoreIcon(eq(FAKE_URL), eq(true));
    }

    @SmallTest
    @Test
    public void testOnStoreInfoClicked() {
        TrackerFactory.setTrackerForTests(mMockTracker);

        mCoordinator.onStoreInfoClicked(mDummyMerchantTrustSignals);
        verify(mMockMetrics, times(1))
                .recordMetricsForBottomSheetOpenedSource(
                        eq(BottomSheetOpenedSource.FROM_PAGE_INFO));
        verify(mMockDetailsTabCoordinator, times(1))
                .requestOpenSheet(
                        any(), any(String.class), mOnBottomSheetDismissedCaptor.capture());
        verify(mMockTracker, times(1))
                .notifyEvent(eq(EventConstants.PAGE_INFO_STORE_INFO_ROW_CLICKED));
        mOnBottomSheetDismissedCaptor.getValue().run();
        verify(mCoordinator, times(0)).maybeShowStoreIcon(any(), anyBoolean());
    }

    @SmallTest
    @Test
    public void testOnlyAbleToShowThreeMessagesInGivenTime() {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MAX_ALLOWED_NUMBER_IN_GIVEN_WINDOW_PARAM,
                "3");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_NUMBER_CHECK_WINDOW_DURATION_PARAM,
                "60000");

        // We won't reach the max allowed number until we show three messages.
        Assert.assertFalse(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());
        mCoordinator.updateShownMessagesTimestamp();
        Assert.assertFalse(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());
        mCoordinator.updateShownMessagesTimestamp();
        Assert.assertFalse(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());
        mCoordinator.updateShownMessagesTimestamp();
        Assert.assertTrue(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());

        // Update the first stored timestamp to beyond the set window then we won't reach the max
        // allowed number.
        String[] timestamps = mSerializedTimestamps.split("_");
        Assert.assertEquals(3, timestamps.length);
        String firstTimestamp = Long.toString(System.currentTimeMillis() - 60001);
        mSerializedTimestamps =
                firstTimestamp + mSerializedTimestamps.substring(timestamps[0].length());
        Assert.assertFalse(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());

        // Show another message and we will drop the first stored timestamp.
        mCoordinator.updateShownMessagesTimestamp();
        timestamps = mSerializedTimestamps.split("_");
        Assert.assertEquals(3, timestamps.length);
        Assert.assertTrue(mCoordinator.hasReachedMaxAllowedMessageNumberInGivenTime());
    }

    @SmallTest
    @Test
    public void testMaybeShowStoreIcon() {
        mCoordinator.setOmniboxIconController(mMockIconController);

        mCoordinator.maybeShowStoreIcon(null, true);
        verify(mMockIconController, times(0))
                .showStoreIcon(
                        eq(mMockWindowAndroid),
                        eq(FAKE_URL),
                        eq(mMockDrawable),
                        anyInt(),
                        anyBoolean());

        mCoordinator.maybeShowStoreIcon(FAKE_URL, true);
        verify(mMockIconController, times(1))
                .showStoreIcon(
                        eq(mMockWindowAndroid),
                        eq(FAKE_URL),
                        eq(mMockDrawable),
                        anyInt(),
                        eq(true));

        mCoordinator.maybeShowStoreIcon(FAKE_URL, false);
        verify(mMockIconController, times(1))
                .showStoreIcon(
                        eq(mMockWindowAndroid),
                        eq(FAKE_URL),
                        eq(mMockDrawable),
                        anyInt(),
                        eq(false));

        mCoordinator.setOmniboxIconController(null);
    }

    private void setMockTrustSignalsData(MerchantInfo merchantInfo) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback callback = (Callback) invocation.getArguments()[2];
                                callback.onResult(merchantInfo);
                                return null;
                            }
                        })
                .when(mMockMerchantTrustDataProvider)
                .getDataForUrl(any(Profile.class), any(GURL.class), any(Callback.class));
    }

    private void setMockTrustSignalsEventData(String hostname, MerchantTrustSignalsEvent event) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback callback = (Callback) invocation.getArguments()[1];
                                callback.onResult(event);
                                return null;
                            }
                        })
                .when(mMockMerchantTrustStorage)
                .load(eq(hostname), any(Callback.class));
    }

    private void verifySchedulingMessage(boolean messageScheduled, boolean shouldExpediteMessage) {
        if (messageScheduled) {
            verify(mMockMerchantMessageScheduler, times(1))
                    .schedule(
                            any(PropertyModel.class),
                            anyDouble(),
                            any(MerchantTrustMessageContext.class),
                            eq(
                                    shouldExpediteMessage
                                            ? MerchantTrustMessageScheduler.MESSAGE_ENQUEUE_NO_DELAY
                                            : (long)
                                                    MerchantViewerConfig
                                                            .getDefaultTrustSignalsMessageDelay()),
                            any(Callback.class));
        } else {
            verify(mMockMerchantMessageScheduler, times(0))
                    .schedule(
                            any(PropertyModel.class),
                            anyDouble(),
                            any(MerchantTrustMessageContext.class),
                            anyLong(),
                            any(Callback.class));
        }
    }
}
