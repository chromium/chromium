// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NtpCardsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
public class NtpCardsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;

    private NtpCardsCoordinator mCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new NtpCardsCoordinator(
                        mContext, mBottomSheetDelegate, new ObservableSupplierImpl<>(mProfile));
    }

    @Test
    @SmallTest
    public void testConstructor() {
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        NtpCardsMediator mediator = mock(NtpCardsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);

        mCoordinator.destroy();
        verify(mediator).destroy();
    }

    @Test
    @SmallTest
    public void testOnAllCardsConfigChanged() {
        NtpCardsMediator mediator = mock(NtpCardsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);

        mCoordinator.onAllCardsConfigChanged(true);
        verify(mediator).onAllCardsConfigChanged(true);

        mCoordinator.onAllCardsConfigChanged(false);
        verify(mediator).onAllCardsConfigChanged(false);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
    public void testToggleVisibility() {
        // TODO(crbug.com/458409311): Remove this test.
        View view = mCoordinator.getViewForTesting();
        assertEquals(View.GONE, view.findViewById(R.id.cards_switch_button).getVisibility());
        assertEquals(View.GONE, view.findViewById(R.id.cards_section_title).getVisibility());
    }

    @Test
    @SmallTest
    public void testToggleVisibility_FeatureEnabled() {
        // TODO(crbug.com/458409311): Remove this test.
        View view = mCoordinator.getViewForTesting();
        assertEquals(View.VISIBLE, view.findViewById(R.id.cards_switch_button).getVisibility());
        assertEquals(View.VISIBLE, view.findViewById(R.id.cards_section_title).getVisibility());
    }
}
