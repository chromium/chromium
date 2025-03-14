// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;

import android.content.Context;
import android.view.View;

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
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NtpCardsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCardsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BottomSheetDelegate mDelegate;
    @Mock PropertyModel mPropertyModel;

    private NtpCardsCoordinator mNtpCardsCoordinator;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mNtpCardsCoordinator = new NtpCardsCoordinator(context, mDelegate, mPropertyModel);
    }

    @Test
    @SmallTest
    public void testConstructor() {
        // Verifies the view is added to view flipper and the back press listener is set in the
        // property model
        verify(mDelegate).registerBottomSheetLayout(eq(NTP_CARDS), any(View.class));
        verify(mPropertyModel)
                .set(eq(NTP_CARDS_BACK_PRESS_HANDLER), any(View.OnClickListener.class));
    }
}
