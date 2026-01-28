// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeListItemView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeListItemViewUnitTest {

    private NtpThemeListItemView mNtpThemeListItemView;
    private ImageView mTrailingIcon;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mNtpThemeListItemView = new NtpThemeListItemView(context, null);
        mTrailingIcon = new ImageView(context);
        mTrailingIcon.setId(R.id.trailing_icon);
        mNtpThemeListItemView.addView(mTrailingIcon);
    }

    @Test
    public void testDestroy() {
        mNtpThemeListItemView.setOnClickListener(v -> {});
        assertTrue(mNtpThemeListItemView.hasOnClickListeners());

        mNtpThemeListItemView.destroy();

        assertFalse(mNtpThemeListItemView.hasOnClickListeners());
    }

    @Test
    public void testUpdateTrailingIcon() {
        // Test for the default section.
        mTrailingIcon.setVisibility(View.GONE);
        mNtpThemeListItemView.updateTrailingIcon(/* visible= */ true, DEFAULT);
        assertEquals(View.VISIBLE, mTrailingIcon.getVisibility());

        mTrailingIcon.setVisibility(View.VISIBLE);
        mNtpThemeListItemView.updateTrailingIcon(/* visible= */ false, DEFAULT);
        assertEquals(View.INVISIBLE, mTrailingIcon.getVisibility());

        // Test for other sections.
        mNtpThemeListItemView.updateTrailingIcon(/* visible= */ true, IMAGE_FROM_DISK);
        ShadowDrawable shadowDrawable = shadowOf(mTrailingIcon.getDrawable());
        assertEquals(R.drawable.ic_check_googblue_24dp, shadowDrawable.getCreatedFromResId());

        mNtpThemeListItemView.updateTrailingIcon(/* visible= */ false, IMAGE_FROM_DISK);
        shadowDrawable = shadowOf(mTrailingIcon.getDrawable());
        assertEquals(R.drawable.forward_arrow_icon, shadowDrawable.getCreatedFromResId());
    }
}
