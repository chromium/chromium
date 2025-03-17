// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_OPTION_CLICK_LISTENER;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NtpCustomizationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpCustomizationMediator mMediator;
    @Mock private PropertyModel mPropertyModel;
    @Mock private View mView;
    @Mock private ViewFlipper mViewFlipper;

    private Context mContext;
    private NtpCustomizationCoordinator mNtpCustomizationCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(mContext, mBottomSheetController, mPropertyModel);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        mNtpCustomizationCoordinator.setViewFlipperForTesting(mViewFlipper);
    }

    @Test
    @SmallTest
    public void testConstructor() {
        // Verifies that the mNtpCardsCoordinator is initialized.
        NtpCustomizationCoordinator coordinator =
                new NtpCustomizationCoordinator(
                        mContext,
                        mBottomSheetController,
                        new PropertyModel(NtpCustomizationViewProperties.ALL_KEYS));

        View ntpCards =
                coordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.new_tab_page_cards_list_item_container);
        ntpCards.performClick();
        assertNotNull(coordinator.getNtpCardsCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        mNtpCustomizationCoordinator.showBottomSheet();
        verify(mMediator).showBottomSheet(eq(MAIN));
    }

    @Test
    @SmallTest
    public void testImplementBottomSheetDelegate() {
        BottomSheetDelegate delegate = mNtpCustomizationCoordinator.getDelegateForTesting();

        // Verifies each implementation calls the corresponding method of the mediator.
        delegate.registerBottomSheetLayout(11, mView);
        verify(mViewFlipper).addView(eq(mView));
        verify(mMediator).registerBottomSheetLayout(11);

        delegate.backPressOnCurrentBottomSheet();
        verify(mMediator).backPressOnCurrentBottomSheet();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mNtpCustomizationCoordinator.destroy();

        assertNull(mPropertyModel.get(NTP_CARDS_OPTION_CLICK_LISTENER));
        assertNull(mPropertyModel.get(NTP_CARDS_BACK_PRESS_HANDLER));
        verify(mViewFlipper).removeAllViews();
        verify(mMediator).destroy();
    }
}
