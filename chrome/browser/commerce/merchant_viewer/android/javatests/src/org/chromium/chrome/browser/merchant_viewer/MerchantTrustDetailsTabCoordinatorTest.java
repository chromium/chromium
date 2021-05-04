// Copyright 2021 The Chromium Authors. All rights reserved.
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
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Tests for {@link MerchantTrustDetailsTabCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MerchantTrustDetailsTabCoordinatorTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BottomSheetController mMockBottomSheetController;

    @Mock
    private View mMockDecorView;

    @Mock
    private Supplier<Tab> mMockTabProvider;

    @Mock
    private MerchantTrustMetrics mMockMetrics;

    @Mock
    private GURL mMockGurl;

    @Mock
    private MerchantTrustDetailsTabMediator mMockMediator;

    @Captor
    private ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserverCaptor;

    @Captor
    private ArgumentCaptor<MerchantTrustDetailsSheetContent> mSheetContentCaptor;

    private static final String DUMMY_SHEET_TITLE = "DUMMY_TITLE";

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private MerchantTrustDetailsTabCoordinator mDetailsTabCoordinator;

    @Before
    public void setUp() {
        mActivity = sActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mWindowAndroid = new WindowAndroid(mActivity); });
        mDetailsTabCoordinator = new MerchantTrustDetailsTabCoordinator(mActivity, mWindowAndroid,
                mMockBottomSheetController, mMockTabProvider, mMockDecorView, mMockMetrics);
        mDetailsTabCoordinator.setMediatorForTesting(mMockMediator);
        requestOpenSheetAndVerify();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDetailsTabCoordinator.destroyWebContents();
            mWindowAndroid.destroy();
        });
    }

    private void requestOpenSheetAndVerify() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mDetailsTabCoordinator.requestOpenSheet(mMockGurl, DUMMY_SHEET_TITLE); });
        verify(mMockBottomSheetController, times(1))
                .addObserver(mBottomSheetObserverCaptor.capture());
        verify(mMockMediator, times(1))
                .init(any(WebContents.class), any(ContentView.class), mSheetContentCaptor.capture(),
                        any(Profile.class));
        verify(mMockDecorView, times(1))
                .addOnLayoutChangeListener(any(OnLayoutChangeListener.class));
        verify(mMockMediator, times(1)).requestShowContent(any(GURL.class), eq(DUMMY_SHEET_TITLE));
    }

    @Test
    @SmallTest
    public void testClose() {
        mDetailsTabCoordinator.close();
        verify(mMockBottomSheetController, times(1))
                .hideContent(eq(mSheetContentCaptor.getValue()), eq(true));
    }

    @Test
    @SmallTest
    public void testBottomSheetObserverOnSheetContentChanged() {
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheetObserverCaptor.getValue().onSheetContentChanged(null); });
        verify(mMockMetrics, times(1))
                .recordMetricsForBottomSheetClosed(eq(StateChangeReason.BACK_PRESS));
        verify(mMockMediator, times(1)).destroyContent();
        verify(mMockDecorView, times(1))
                .removeOnLayoutChangeListener(any(OnLayoutChangeListener.class));
        verify(mMockBottomSheetController, times(1))
                .removeObserver(eq(mBottomSheetObserverCaptor.getValue()));
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
        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(SheetState.PEEK);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetPeeked();
        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(SheetState.HALF);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetHalfOpened();
        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(SheetState.FULL);
        verify(mMockMetrics, times(1)).recordMetricsForBottomSheetFullyOpened();
    }
}