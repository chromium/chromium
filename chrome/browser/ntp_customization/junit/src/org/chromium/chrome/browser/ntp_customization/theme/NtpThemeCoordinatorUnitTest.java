// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NtpThemeCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;

    private Context mContext;
    private NtpThemeCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mCoordinator = new NtpThemeCoordinator(mContext, mBottomSheetDelegate, mProfile);
    }

    @Test
    public void testConstructor() {
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    public void testRegisterBottomSheetLayout() {
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(
                        eq(NtpThemeCoordinator.NTPThemeBottomSheetSection.THEME_COLLECTIONS),
                        any());
    }

    @Test
    public void testDestroy() {
        NtpThemeMediator mediator = mock(NtpThemeMediator.class);
        mCoordinator.setMediatorForTesting(mediator);
        mCoordinator.destroy();
        verify(mediator).destroy();

        NtpThemeBottomSheetView ntpThemeBottomSheetView = mock(NtpThemeBottomSheetView.class);
        mCoordinator.setNtpThemeBottomSheetViewForTesting(ntpThemeBottomSheetView);
        mCoordinator.destroy();
        verify(ntpThemeBottomSheetView).destroy();
    }
}
