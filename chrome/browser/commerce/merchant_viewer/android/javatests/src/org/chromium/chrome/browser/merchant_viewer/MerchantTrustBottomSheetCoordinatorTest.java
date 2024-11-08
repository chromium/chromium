// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import androidx.test.filters.SmallTest;

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

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.GURL;

/** Tests for {@link MerchantTrustBottomSheetCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class MerchantTrustBottomSheetCoordinatorTest extends BlankUiTestActivityTestCase {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;

    @Mock private View mMockDecorView;

    @Mock private Supplier<Tab> mMockTabProvider;

    @Mock private MerchantTrustMetrics mMockMetrics;

    @Mock private GURL mMockGurl;

    @Mock private MerchantTrustBottomSheetMediator mMockMediator;

    @Mock private Runnable mMockOnBottomSheetDismissed;

    @Captor private ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserverCaptor;

    @Captor private ArgumentCaptor<MerchantTrustBottomSheetContent> mSheetContentCaptor;

    private static final String DUMMY_SHEET_TITLE = "DUMMY_TITLE";

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private MerchantTrustBottomSheetCoordinator mDetailsTabCoordinator;

    @Before
    public void setUp() {
        mActivity = getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = new WindowAndroid(mActivity);
                    mDetailsTabCoordinator =
                            new MerchantTrustBottomSheetCoordinator(
                                    mActivity,
                                    mWindowAndroid,
                                    mMockBottomSheetController,
                                    mMockTabProvider,
                                    mMockDecorView,
                                    mMockMetrics,
                                    IntentRequestTracker.createFromActivity(mActivity),
                                    new ObservableSupplierImpl<Profile>());
                });
        mDetailsTabCoordinator.setMediatorForTesting(mMockMediator);
        requestOpenSheetAndVerify();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDetailsTabCoordinator.destroySheet();
                    mWindowAndroid.destroy();
                });
    }

    private void requestOpenSheetAndVerify() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDetailsTabCoordinator.requestOpenSheet(
                            mMockGurl, DUMMY_SHEET_TITLE, mMockOnBottomSheetDismissed);
                });
        verify(mMockMediator, times(1))
                .setupSheetWebContents(any(ThinWebView.class), any(PropertyModel.class));
        verify(mMockBottomSheetController, times(1))
                .addObserver(mBottomSheetObserverCaptor.capture());
        verify(mMockDecorView, times(1))
                .addOnLayoutChangeListener(any(OnLayoutChangeListener.class));
        verify(mMockMediator, times(1)).navigateToUrl(eq(mMockGurl), eq(DUMMY_SHEET_TITLE));
        verify(mMockBottomSheetController, times(1))
                .requestShowContent(mSheetContentCaptor.capture(), eq(true));
    }

    @Test
    @SmallTest
    public void testCloseSheet() {
        mDetailsTabCoordinator.closeSheet();
        verify(mMockBottomSheetController, times(1))
                .hideContent(eq(mSheetContentCaptor.getValue()), eq(true));
    }

    @Test
    @SmallTest
    public void testBottomSheetObserverOnSheetContentChanged() {
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetObserverCaptor.getValue().onSheetContentChanged(null);
                });
        verify(mMockMetrics, times(1))
                .recordMetricsForBottomSheetClosed(eq(StateChangeReason.BACK_PRESS));
        verify(mMockOnBottomSheetDismissed, times(1)).run();
        verify(mMockDecorView, times(1))
                .removeOnLayoutChangeListener(any(OnLayoutChangeListener.class));
        verify(mMockBottomSheetController, times(1))
                .removeObserver(eq(mBottomSheetObserverCaptor.getValue()));
        verify(mMockBottomSheetController, times(1))
                .hideContent(eq(mSheetContentCaptor.getValue()), eq(true));
        verify(mMockMediator, times(1)).destroyWebContents();
    }

    @Test
    @SmallTest
    public void testBottomSheetObserverOnSheetOpened() {
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.PROMOTE_TAB);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetHalfOpened();
    }

    @Test
    @SmallTest
    public void testBottomSheetObserverOnSheetStateChanged() {
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetPeeked();
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetHalfOpened();
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetFullyOpened();
    }
}
