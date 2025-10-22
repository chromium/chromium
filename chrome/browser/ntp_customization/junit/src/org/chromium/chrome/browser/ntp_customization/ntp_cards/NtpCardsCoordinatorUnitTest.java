// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NtpCardsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
public class NtpCardsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;

    private NtpCardsCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mCoordinator =
                new NtpCardsCoordinator(
                        context, mBottomSheetDelegate, new ObservableSupplierImpl<>(mProfile));
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
}
