// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit tests for {@link NtpCustomizationMainBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class NtpCustomizationMainBottomSheetContentUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private View mView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_main_bottom_sheet, /* root= */ null);
    }

    @Test
    @SmallTest
    public void testBasics() {
        NtpCustomizationMainBottomSheetContent mNtpCustomizationMainBottomSheetContent =
                new NtpCustomizationMainBottomSheetContent(mView);

        assertNotNull(mNtpCustomizationMainBottomSheetContent.getContentView());
        assertNull(mNtpCustomizationMainBottomSheetContent.getToolbarView());
        assertEquals(0, mNtpCustomizationMainBottomSheetContent.getVerticalScrollOffset());
        assertEquals(
                BottomSheetContent.ContentPriority.HIGH,
                mNtpCustomizationMainBottomSheetContent.getPriority());
        assertFalse(mNtpCustomizationMainBottomSheetContent.swipeToDismissEnabled());
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mNtpCustomizationMainBottomSheetContent.getPeekHeight());
        assertEquals(
                (float) BottomSheetContent.HeightMode.WRAP_CONTENT,
                mNtpCustomizationMainBottomSheetContent.getFullHeightRatio(),
                MathUtils.EPSILON);

        assertEquals(
                mContext.getString(
                        R.string.ntp_customization_main_bottom_sheet_content_description),
                mNtpCustomizationMainBottomSheetContent.getSheetContentDescription(mContext));
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_opened_full,
                mNtpCustomizationMainBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.ntp_customization_main_bottom_sheet_closed,
                mNtpCustomizationMainBottomSheetContent.getSheetClosedAccessibilityStringId());
    }
}
