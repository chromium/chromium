// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
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
                new NtpCustomizationCoordinator(mContext, mBottomSheetController);
        mNtpCustomizationCoordinator.setViewFlipperForTesting(mViewFlipper);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        mNtpCustomizationCoordinator.showBottomSheet();
        verify(mMediator).showBottomSheet(eq(MAIN));
    }

    @Test
    @SmallTest
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
    @SmallTest
    public void testGetOptionClickListener() {
        View.OnClickListener listener =
                mNtpCustomizationCoordinator.getOptionClickListener(NTP_CARDS);

        listener.onClick(new View(mContext));
        assertNotNull(mNtpCustomizationCoordinator.getNtpCardsCoordinatorForTesting());
        verify(mMediator).showBottomSheet(eq(NTP_CARDS));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        NtpCardsCoordinator ntpCardsCoordinator = mock(NtpCardsCoordinator.class);
        mNtpCustomizationCoordinator.setNtpCardsCoordinatorForTesting(ntpCardsCoordinator);

        mNtpCustomizationCoordinator.destroy();

        verify(mViewFlipper).removeAllViews();
        verify(mMediator).destroy();
        verify(ntpCardsCoordinator).destroy();
    }
}
