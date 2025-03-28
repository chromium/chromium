// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link NtpCardsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCardsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private NtpCardsCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mCoordinator = new NtpCardsCoordinator(context, mock(BottomSheetDelegate.class));
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
