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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.messages.DismissReason;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link MerchantTrustSignalsCoordinator}.
 *
 * NOTE: This test is temporarily skipped for SDK version < 23 (M) since the test attempts to mock
 * {@link WindowAndroid} which has a dependency on android.View.Display.Mode which is not supported
 * on versions prior to Android M.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_MERCHANT_VIEWER + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M)
@DisableIf.Device(type = {UiDisableIf.TABLET})
public class MerchantTrustSignalsCoordinatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

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

    @Mock
    private MerchantTrustDetailsTabCoordinator mMockDetailsTabCoordinator;

    @Captor
    private ArgumentCaptor<Callback> mOnMessageEnqueuedCallbackCaptor;

    @Mock
    private ObservableSupplier<Profile> mMockProfileSupplier;

    @Mock
    private NavigationHandle mNavigationHandle;

    private MerchantTrustSignals mDummyMerchantTrustSignals =
            MerchantTrustSignals.newBuilder()
                    .setMerchantStarRating(4.5f)
                    .setMerchantCountRating(100)
                    .setMerchantDetailsPageUrl("http://dummy/url")
                    .build();
    @Before
    public void setUp() {
        doReturn(mTabModelFilterProvider).when(mMockTabModelSelector).getTabModelFilterProvider();
        doReturn("Test").when(mMockResources).getString(anyInt());
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
        doReturn(mMockGurl).when(mNavigationHandle).getUrl();
        doReturn(true).when(mNavigationHandle).isInMainFrame();
    }

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
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));

        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));

        verify(mMockMerchantTrustStorage, times(1)).delete(eq(mMockMerchantTrustSignalsEvent));
        verify(mMockMerchantTrustDataProvider, times(1))
                .getDataForNavigationHandle(eq(mNavigationHandle), any(Callback.class));
    }

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
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantMessageScheduler, never())
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

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
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
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

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));
        verify(mMockMerchantMessageScheduler, never())
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessageWithScheduledMessage() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .expedite(mOnMessageEnqueuedCallbackCaptor.capture());
    }

    @SmallTest
    @Test
    public void testMaybeDisplayMessageWithScheduledMessageForDifferentHost() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        NavigationHandle mockNavigationHandle2 = mock(NavigationHandle.class);
        doReturn(mMockGurl2).when(mockNavigationHandle2).getUrl();

        doReturn(new MerchantTrustMessageContext(mockNavigationHandle2, mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        setMockTrustSignalsData(mDummyMerchantTrustSignals);
        setMockTrustSignalsEventData("fake_host", null);
        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1))
                .clear(eq(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN));

        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq((long) MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY
                                        .getValue()),
                        any(Callback.class));
    }

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
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));

        verify(mMockMerchantTrustStorage, never()).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    public void testOnMessageEnqueued() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        coordinator.onMessageEnqueued(null);
        verify(mMockMerchantTrustStorage, times(0)).save(any(MerchantTrustSignalsEvent.class));

        coordinator.onMessageEnqueued(
                new MerchantTrustMessageContext(mNavigationHandle, mMockWebContents));
        verify(mMockMerchantTrustStorage, times(1)).save(any(MerchantTrustSignalsEvent.class));
    }

    @SmallTest
    @Test
    public void testOnMessageDismissed() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        coordinator.onMessageDismissed(DismissReason.TIMER);
        verify(mMockMetrics, times(1)).recordMetricsForMessageDismissed(eq(DismissReason.TIMER));
    }

    @SmallTest
    @Test
    public void testOnMessagePrimaryAction() {
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        coordinator.onMessagePrimaryAction(mDummyMerchantTrustSignals);
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

    private void setNavigationHandle(GURL url) {
        doReturn(url).when(mNavigationHandle).getUrl();
    }

    private MerchantTrustSignalsCoordinator getCoordinatorUnderTest() {
        return new MerchantTrustSignalsCoordinator(mMockContext, mMockWindowAndroid,
                mMockBottomSheetController, mMockDecorView, mMockTabModelSelector,
                mMockMerchantMessageScheduler, mMockTabProvider, mMockMerchantTrustDataProvider,
                mMockMetrics, mMockDetailsTabCoordinator, mMockProfileSupplier,
                mMockMerchantTrustStorageFactory);
    }
}