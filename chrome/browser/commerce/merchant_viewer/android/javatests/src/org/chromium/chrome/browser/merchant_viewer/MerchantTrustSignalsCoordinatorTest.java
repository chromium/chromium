// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.messages.DismissReason;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link MerchantTrustSignalsCoordinator}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_MERCHANT_VIEWER + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class MerchantTrustSignalsCoordinatorTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    private TabModelSelector mMockTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private MerchantTrustMessageScheduler mMockMerchantMessageScheduler;

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private Context mMockContext;

    @Mock
    private BottomSheetController mMockBottomSheetController;

    @Mock
    private View mMockDecorView;

    @Mock
    private Supplier<Tab> mMockTabProvider;

    @Mock
    private WindowAndroid mMockWindowAndroid;

    @Mock
    private Resources mMockResources;

    @Mock
    private DisplayAndroid mMockDisplayAndroid;

    @Mock
    private MerchantTrustMetrics mMockMetrics;

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

    @Captor
    private ArgumentCaptor<Callback> mOnMessageEnqueuedCallbackCaptor;

    @Mock
    private ObservableSupplier<Profile> mMockProfileSupplier;

    @Mock
    private MerchantTrustDetailsTabCoordinator mMockDetailsTabCoordinator;

    private MerchantTrustSignals mDummyMerchantTrustSignals =
            MerchantTrustSignals.newBuilder()
                    .setMerchantStarRating(4.5f)
                    .setMerchantCountRating(100)
                    .setMerchantDetailsPageUrl("http://dummy/url")
                    .build();
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFilterProvider).when(mMockTabModelSelector).getTabModelFilterProvider();
        doReturn("Test").when(mMockResources).getString(anyInt(), anyObject());
        doReturn("Test").when(mMockResources).getQuantityString(anyInt(), anyInt(), anyObject());
        doReturn(100).when(mMockResources).getDimensionPixelSize(any(Integer.class));
        doReturn(mMockResources).when(mMockContext).getResources();
        doReturn(1f).when(mMockDisplayAndroid).getDipScale();
        doReturn(mMockDisplayAndroid).when(mMockWindowAndroid).getDisplay();
        doReturn("fake_host").when(mMockGurl).getHost();
        doReturn("different_host").when(mMockGurl2).getHost();
        doReturn(mMockMerchantTrustStorage)
                .when(mMockMerchantTrustStorageFactory)
                .getForLastUsedProfile();
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    public void testMaybeDisplayMessage() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", mMockMerchantTrustSignalsEvent);

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));

        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));

        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForUrl(eq(mMockGurl), any(Callback.class));
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/60000"})
    public void testMaybeDisplayMessageLastEventWithinTimeWindow() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.SECONDS.toMillis(10))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", mMockMerchantTrustSignalsEvent);

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantMessageScheduler, never())
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    public void testMaybeDisplayMessageFirstTime() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", null);

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:trust_signals_message_window_duration_ms/-1"})
    public void testMaybeDisplayMessageNoMerchantTrustData() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(null);
        setMockTrustSignalsEventData("fake_host", null);

        // doReturn(mDummyMerchantTrustSignalsEvent)
        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantMessageScheduler, never())
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMaybeDisplayMessageWithScheduledMessage() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(new MerchantTrustMessageContext(mMockGurl, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .expedite(mOnMessageEnqueuedCallbackCaptor.capture());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMaybeDisplayMessageWithScheduledMessageForDifferentHost() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(new MerchantTrustMessageContext(mMockGurl2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", null);
        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));

        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMaybeDisplayMessageWithInvalidStorage() {
        doReturn(null).when(mMockMerchantTrustStorageFactory).getForLastUsedProfile();

        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();
        doReturn(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1))
                .when(mMockMerchantTrustSignalsEvent)
                .getTimestamp();
        doReturn("fake_host").when(mMockMerchantTrustSignalsEvent).getKey();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", mMockMerchantTrustSignalsEvent);

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mMockGurl, mMockWebContents));

        verify(mMockMerchantTrustStorage, never()).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    public void testOnMessageDismissed() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        coordinator.onMessageDismissed(DismissReason.TIMER);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.TIMER));
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
                .getDataForUrl(any(GURL.class), any(Callback.class));
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

    private MerchantTrustSignalsCoordinator getCoordinatorUnderTest() {
        return new MerchantTrustSignalsCoordinator(mMockContext, mMockWindowAndroid,
                mMockBottomSheetController, mMockDecorView, mMockTabModelSelector,
                mMockMerchantMessageScheduler, mMockTabProvider, mMockMerchantTrustDataProvider,
                mMockMetrics, mMockDetailsTabCoordinator, mMockProfileSupplier,
                mMockMerchantTrustStorageFactory);
    }
}