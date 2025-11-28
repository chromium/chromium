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

import static org.chromium.chrome.browser.flags.ChromeFeatureList.HOME_MODULE_PREF_REFACTOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/** Unit tests for {@link NtpCustomizationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(HOME_MODULE_PREF_REFACTOR)
public class NtpCustomizationCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpCustomizationMediator mMediator;
    @Mock private ViewFlipper mViewFlipper;
    @Mock private NtpThemeCoordinator mNtpThemeCoordinator;

    private Context mContext;
    private NtpCustomizationCoordinator mNtpCustomizationCoordinator;
    private View mContentView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_ntp_cards_bottom_sheet,
                                /* root= */ null);
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
        delegate.registerBottomSheetLayout(11, mContentView);
        verify(mViewFlipper).addView(eq(mContentView));
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
                mNtpCustomizationCoordinator.initBottomSheetContent(mContentView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).backPressOnCurrentBottomSheet();

        // Verifies that if the bottom sheet type is not MAIN, dismissBottomSheet() is called
        // to handle back presses.
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), NTP_CARDS);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        bottomSheetContent = mNtpCustomizationCoordinator.initBottomSheetContent(mContentView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).dismissBottomSheet(/* animate= */ eq(true));

        clearInvocations(mMediator);
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(
                        mContext, mBottomSheetController, mock(Supplier.class), FEED);
        mNtpCustomizationCoordinator.setMediatorForTesting(mMediator);
        bottomSheetContent = mNtpCustomizationCoordinator.initBottomSheetContent(mContentView);
        bottomSheetContent.handleBackPress();
        verify(mMediator).dismissBottomSheet(/* animate= */ eq(true));
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

    @Test
    public void testBottomSheetDelegateImplementation_showBottomSheetForTheme() {
        BottomSheetDelegate delegate =
                mNtpCustomizationCoordinator.getBottomSheetDelegateForTesting();
        mNtpCustomizationCoordinator.setNtpThemeCoordinatorForTesting(mNtpThemeCoordinator);

        delegate.showBottomSheet(THEME_COLLECTIONS);
        verify(mMediator).showBottomSheet(eq(THEME_COLLECTIONS));
        verify(mNtpThemeCoordinator).initializeBottomSheetContent(eq(THEME_COLLECTIONS));

        delegate.showBottomSheet(SINGLE_THEME_COLLECTION);
        verify(mMediator).showBottomSheet(eq(SINGLE_THEME_COLLECTION));
        verify(mNtpThemeCoordinator).initializeBottomSheetContent(eq(SINGLE_THEME_COLLECTION));
    }
}
