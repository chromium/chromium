// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;

/** Unit tests for {@link MvtSettingsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class MvtSettingsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BottomSheetDelegate mBottomSheetDelegate;
    private MvtSettingsCoordinator mCoordinator;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        org.chromium.chrome.browser.ntp_customization.R.style
                                .Theme_BrowserUI_DayNight);
        mCoordinator = new MvtSettingsCoordinator(context, mBottomSheetDelegate);
    }

    @Test
    public void testConstructor() {
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(
                        eq(NtpCustomizationCoordinator.BottomSheetType.MVT), any(View.class));
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    public void testDestroy() {
        MvtSettingsMediator mediator = mock(MvtSettingsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);
        mCoordinator.destroy();
        verify(mediator).destroy();
    }
}
