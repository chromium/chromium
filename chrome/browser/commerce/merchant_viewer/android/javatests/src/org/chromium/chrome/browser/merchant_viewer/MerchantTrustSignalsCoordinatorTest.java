// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.DismissReason;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link MerchantTrustSignalsCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustSignalsCoordinatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Context mMockContext;

    @Mock
    private Resources mMockResources;

    @Mock
    private MerchantTrustMessageScheduler mMockMerchantMessageScheduler;

    @Mock
    private ObservableSupplier<Tab> mMockTabProvider;

    @Mock
    private ObservableSupplier<Profile> mMockProfileSupplier;

    @Mock
    private Profile mMockProfile;

    @Mock
    private MerchantTrustMetrics mMockMetrics;

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private GURL mMockGurl;

    @Mock
    private GURL mMockGurl2;

    @Mock
    private MerchantTrustSignalsDataProvider mMockMerchantTrustDataProvider;

    @Mock
    private MerchantTrustSignalsEventStorage mMockMerchantTrustStorage;

    @Mock
    private MerchantTrustSignalsStorageFactory mMockMerchantTrustStorageFactory;

    @Mock
    private MerchantTrustSignalsEvent mMockMerchantTrustSignalsEvent;

    @Mock
    private MerchantTrustBottomSheetCoordinator mMockDetailsTabCoordinator;

    @Mock
    private NavigationHandle mMockNavigationHandle;

    @Mock
    private NavigationHandle mMockNavigationHandle2;

    private static final String FAKE_HOST = "fake_host";
    private static final String DIFFERENT_HOST = "different_host";

    private MerchantTrustSignals mDummyMerchantTrustSignals = MerchantTrustSignals.newBuilder()
                                                                      .setMerchantStarRating(4.5f)
                                                                      .setMerchantCountRating(100)
                                                                      .setMerchantDetailsPageUrl("")
                                                                      .build();
    private MerchantTrustSignalsCoordinator mCoordinator;
    private FeatureList.TestValues mTestValues;

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

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData(FAKE_HOST, mMockMerchantTrustSignalsEvent);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM, "-1");
        FeatureList.setTestValues(mTestValues);

        mCoordinator = spy(new MerchantTrustSignalsCoordinator(mMockContext,
                mMockMerchantMessageScheduler, mMockTabProvider, mMockMerchantTrustDataProvider,
                mMockProfileSupplier, mMockMetrics, mMockDetailsTabCoordinator,
                mMockMerchantTrustStorageFactory));
        doReturn(0.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        verify(mMockMerchantTrustStorageFactory, times(1)).destroy();
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage() {
        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_LastEventWithinTimeWindow() {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM, "60000");
        doReturn(System.currentTimeMillis() - TimeUnit.SECONDS.toMillis(10))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(0))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(0))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_FirstTime() {
        setMockTrustSignalsEventData(FAKE_HOST, null);

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_NoMerchantTrustData() {
        setMockTrustSignalsData(null);

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(0))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithInvalidStorage() {
        doReturn(null).when(mMockMerchantTrustStorageFactory).getForLastUsedProfile();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(0))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(0))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithInvalidNavigationHandler() {
        doReturn(null).when(mMockNavigationHandle).getUrl();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(0))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(0))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithSiteEngagementAboveThreshold() {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM, "true");
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM, "80.0");
        doReturn(90.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(0))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(0))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithSiteEngagementBelowThreshold() {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM, "true");
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM, "80.0");
        doReturn(70.0)
                .when(mCoordinator)
                .getSiteEngagementScore(any(Profile.class), any(String.class));

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.getDefaultTrustSignalsMessageDelay()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithScheduledMessage() {
        doReturn(FAKE_HOST).when(mMockGurl2).getHost();
        doReturn(DIFFERENT_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1)).expedite(any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithScheduledMessage_ForSameUrl() {
        doReturn(FAKE_HOST).when(mMockGurl2).getHost();
        doReturn(FAKE_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(0)).expedite(any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessage_WithScheduledMessage_ForDifferentHost() {
        doReturn(DIFFERENT_HOST).when(mMockGurl2).getHost();
        doReturn(DIFFERENT_HOST).when(mMockGurl2).getSpec();
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(0)).expedite(any(Callback.class));
    }

    @SmallTest
    @Test
    public void testOnMessageEnqueued() {
        mCoordinator.onMessageEnqueued(null);
        verify(mMockMerchantTrustStorage, times(0)).save(any(MerchantTrustSignalsEvent.class));

        mCoordinator.onMessageEnqueued(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));
        verify(mMockMerchantTrustStorage, times(1)).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    public void testOnMessageDismissed() {
        mCoordinator.onMessageDismissed(DismissReason.TIMER);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.TIMER));
    }

    @SmallTest
    @Test
    public void testOnMessagePrimaryAction() {
        mCoordinator.onMessagePrimaryAction(mDummyMerchantTrustSignals);
        verify(mMockMetrics, times(1)).recordMetricsForMessageTapped();
        verify(mMockDetailsTabCoordinator, times(1))
                .requestOpenSheet(any(GURL.class), any(String.class));
    }

    private void setMockTrustSignalsData(MerchantTrustSignals trustSignalsData) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(trustSignalsData);
                return null;
            }
        })
                .when(mMockMerchantTrustDataProvider)
                .getDataForNavigationHandle(any(NavigationHandle.class), any(Callback.class));
    }

    private void setMockTrustSignalsEventData(String hostname, MerchantTrustSignalsEvent event) {
        doAnswer(new Answer<Void>() {
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
}
