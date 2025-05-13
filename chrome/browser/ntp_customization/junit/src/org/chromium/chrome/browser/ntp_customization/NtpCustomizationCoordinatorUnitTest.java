// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link NtpCustomizationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpCustomizationMediator mMediator;
    @Mock private View mView;
    @Mock private ViewFlipper mViewFlipper;

    private Context mContext;
    private NtpCustomizationCoordinator mNtpCustomizationCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), MAIN);
        mNtpCustomizationCoordinator.setViewFlipperForTesting(mViewFlipper);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    public void testShowBottomSheet() {
        mNtpCustomizationCoordinator.showBottomSheet();
        verify(mMediator).showBottomSheet(eq(MAIN));
    }

    @Test
    public void testBottomSheetDelegateImplementation() {
        BottomSheetDelegate delegate =
                mNtpCustomizationCoordinator.getBottomSheetDelegateForTesting();

        // Verifies each implementation calls the corresponding method of the mediator.
        delegate.registerBottomSheetLayout(11, mView);
        verify(mViewFlipper).addView(eq(mView));
        verify(mMediator).registerBottomSheetLayout(11);

        delegate.backPressOnCurrentBottomSheet();
        verify(mMediator).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testShouldShowAloneInBottomSheetDelegate() {
        // Verifies that if being requested to show the main bottom sheet with its full navigation
        // flow, shouldShowAlone() should return False.
        assertFalse(
                mNtpCustomizationCoordinator.getBottomSheetDelegateForTesting().shouldShowAlone());

        // Verifies that if being requested to show the NTP Cards bottom sheet alone,
        // shouldShowAlone returns True.
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), NTP_CARDS);
        assertTrue(
                mNtpCustomizationCoordinator.getBottomSheetDelegateForTesting().shouldShowAlone());

        // Verifies that if being requested to show the Feed settings bottom sheet alone,
        // shouldShowAlone return True.
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), FEED);
        assertTrue(
                mNtpCustomizationCoordinator.getBottomSheetDelegateForTesting().shouldShowAlone());
    }

    @Test
    public void testInitBottomSheetContent() {
        // Verifies that if the bottom sheet type is MAIN, backPressOnCurrentBottomSheet() is called
        // to handle back presses.
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), MAIN);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        BottomSheetContent bottomSheetContent =
                mNtpCustomizationCoordinator.initBottomSheetContent(mView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).backPressOnCurrentBottomSheet();

        // Verifies that if the bottom sheet type is not MAIN, dismissBottomSheet() is called
        // to handle back presses.
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), NTP_CARDS);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        bottomSheetContent = mNtpCustomizationCoordinator.initBottomSheetContent(mView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).dismissBottomSheet();

        clearInvocations(mMediator);
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), FEED);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        bottomSheetContent = mNtpCustomizationCoordinator.initBottomSheetContent(mView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).dismissBottomSheet();
    }

    @Test
    public void testGetOptionClickListener() {
        View.OnClickListener listener =
                mNtpCustomizationCoordinator.getOptionClickListener(NTP_CARDS);

        listener.onClick(new View(mContext));
        assertNotNull(mNtpCustomizationCoordinator.getNtpCardsCoordinatorForTesting());
        verify(mMediator).showBottomSheet(eq(NTP_CARDS));
    }

    @Test
    public void testDestroy() {
        NtpCardsCoordinator ntpCardsCoordinator = mock(NtpCardsCoordinator.class);
        mNtpCustomizationCoordinator.setNtpCardsCoordinatorForTesting(ntpCardsCoordinator);

        mNtpCustomizationCoordinator.destroy();

        verify(mViewFlipper).removeAllViews();
        verify(mMediator).destroy();
        verify(ntpCardsCoordinator).destroy();
    }
}
