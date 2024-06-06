// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertFalse;

import android.app.Activity;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit tests for {@link AutofillSaveIbanBottomSheetContent} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveIbanBottomSheetContentTest {
    private Activity mActivity;
    private AutofillSaveIbanBottomSheetContent mContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // set a MaterialComponents theme which is required for the `OutlinedBox` text field.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContent = new AutofillSaveIbanBottomSheetContent(mActivity);
    }

    @Test
    public void testBottomSheetHasNoToolbar() {
        assertThat(mContent.getToolbarView(), nullValue());
    }

    @Test
    public void testCannotSwipeToDismissBottomSheet() {
        assertFalse(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testNoVerticalScrollOffset() {
        assertThat(mContent.getVerticalScrollOffset(), equalTo(0));
    }

    @Test
    public void testBottomSheetPriority() {
        assertThat(mContent.getPriority(), equalTo(BottomSheetContent.ContentPriority.HIGH));
    }

    @Test
    public void testBottomSheetCannotPeek() {
        assertThat(mContent.getPeekHeight(), equalTo(BottomSheetContent.HeightMode.DISABLED));
    }

    @Test
    public void testHalfHeightRatio() {
        assertThat(
                mContent.getHalfHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.DISABLED));
    }

    @Test
    public void testFullHeightRatio() {
        assertThat(
                mContent.getFullHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.WRAP_CONTENT));
    }

    @Test
    public void testBottomSheetAccessibilityContentDescription() {
        // TODO(b/309163431): Support a11y.
        assertThat(mContent.getSheetContentDescriptionStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetHalfHeightAccessibilityContentDescription() {
        // TODO(b/309163431): Support a11y.
        assertThat(mContent.getSheetHalfHeightAccessibilityStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetFullHeightAccessibilityContentDescription() {
        // TODO(b/309163431): Support a11y.
        assertThat(mContent.getSheetFullHeightAccessibilityStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetClosedAccessibilityContentDescription() {
        // TODO(b/309163431): Support a11y.
        assertThat(mContent.getSheetClosedAccessibilityStringId(), equalTo(R.string.ok));
    }
}
