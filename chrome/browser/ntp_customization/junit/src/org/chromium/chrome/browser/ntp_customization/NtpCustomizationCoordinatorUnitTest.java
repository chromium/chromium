// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link NtpCustomizationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;

    private NtpCustomizationCoordinator mNtpCustomizationCoordinator;

    @Before
    public void setUp() {
        Context mContext = ApplicationProvider.getApplicationContext();
        mNtpCustomizationCoordinator =
                new NtpCustomizationCoordinator(mContext, mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        mNtpCustomizationCoordinator.showBottomSheet();
        verify(mBottomSheetController)
                .requestShowContent(
                        any(NtpCustomizationMainBottomSheetContent.class), /* animate= */ eq(true));
    }
}
