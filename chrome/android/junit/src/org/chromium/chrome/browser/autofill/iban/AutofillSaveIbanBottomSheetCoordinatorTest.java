// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetCoordinatorTest {
    private static final String IBAN_LABEL = "CH56 **** **** **** *800 9";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillSaveIbanBottomSheetBridge mBridge;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;

    private Activity mActivity;
    private AutofillSaveIbanBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        // set a MaterialComponents theme which is required for the `OutlinedBox` text field.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new AutofillSaveIbanBottomSheetCoordinator(
                        mBridge,
                        mActivity,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel);
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent(IBAN_LABEL);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testRequestShowContent_requestsShowEmptyString() {
        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class, () -> mCoordinator.requestShowContent(""));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("IBAN label passed from C++ should not be NULL or empty.");
    }

    @Test
    public void testDestroy() {
        mCoordinator.requestShowContent(IBAN_LABEL);
        mCoordinator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true));
    }
}
