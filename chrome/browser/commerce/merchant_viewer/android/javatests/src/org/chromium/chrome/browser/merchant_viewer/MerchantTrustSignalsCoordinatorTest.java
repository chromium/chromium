// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.messages.DismissReason;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link MerchantTrustSignalsCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.COMMERCE_MERCHANT_VIEWER + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class MerchantTrustSignalsCoordinatorTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MerchantTrustMessageScheduler mMockMerchantMessageScheduler;

    @Mock
    private ObservableSupplier<Tab> mMockTabProvider;

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

    private MerchantTrustSignals mDummyMerchantTrustSignals =
            MerchantTrustSignals.newBuilder()
                    .setMerchantStarRating(4.5f)
                    .setMerchantCountRating(100)
                    .setMerchantDetailsPageUrl("http://dummy/url")
                    .build();
    private MerchantTrustSignalsCoordinator mCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        doReturn("fake_host").when(mMockGurl).getHost();
        doReturn("different_host").when(mMockGurl2).getHost();
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
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", mMockMerchantTrustSignalsEvent);

        mActivity = sActivityTestRule.getActivity();
        mCoordinator = new MerchantTrustSignalsCoordinator(mActivity, mMockMerchantMessageScheduler,
                mMockTabProvider, mMockMerchantTrustDataProvider, mMockMetrics,
                mMockDetailsTabCoordinator, mMockMerchantTrustStorageFactory);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        verify(mMockMerchantTrustStorageFactory, times(1)).destroy();
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    @DisabledTest(message = "https://crbug.com/1211897")
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
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/60000"})
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testMaybeDisplayMessage_LastEventWithinTimeWindow() {
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
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testMaybeDisplayMessage_FirstTime() {
        setMockTrustSignalsEventData("fake_host", null);

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantTrustStorage, times(0)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mMockNavigationHandle), any(Callback.class));
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    @DisabledTest(message = "https://crbug.com/1211897")
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
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    @DisabledTest(message = "https://crbug.com/1211897")
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
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
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
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testMaybeDisplayMessage_WithScheduledMessage() {
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1)).expedite(any(Callback.class));
    }

    @SmallTest
    @Test
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testMaybeDisplayMessage_WithScheduledMessage_ForDifferentHost() {
        doReturn(new MerchantTrustMessageContext(mMockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        mCoordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(0)).expedite(any(Callback.class));
    }

    @SmallTest
    @Test
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testOnMessageEnqueued() {
        mCoordinator.onMessageEnqueued(null);
        verify(mMockMerchantTrustStorage, times(0)).save(any(MerchantTrustSignalsEvent.class));

        mCoordinator.onMessageEnqueued(
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents));
        verify(mMockMerchantTrustStorage, times(1)).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    @DisabledTest(message = "https://crbug.com/1211897")
    public void testOnMessageDismissed() {
        mCoordinator.onMessageDismissed(DismissReason.TIMER);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.TIMER));
    }

    @SmallTest
    @Test
    @DisabledTest(message = "https://crbug.com/1211897")
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
