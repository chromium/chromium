// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

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
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mNtpThemeListItemView = new NtpThemeListItemView(activity, null);
        mTrailingIcon = new ImageView(activity);
        mTrailingIcon.setId(R.id.trailing_icon);
        mNtpThemeListItemView.addView(mTrailingIcon);
        activity.setContentView(mNtpThemeListItemView);
    }

    @Test
    public void testDestroy() {
        mNtpThemeListItemView.setOnClickListener(v -> {});
        assertTrue(mNtpThemeListItemView.hasOnClickListeners());

        mNtpThemeListItemView.destroy();

        assertFalse(mNtpThemeListItemView.hasOnClickListeners());
    }

    @Test
    public void testSetTrailingIconVisibility() {
        mTrailingIcon.setVisibility(View.GONE);
        mNtpThemeListItemView.setTrailingIconVisibility(true);
        assertEquals(View.VISIBLE, mTrailingIcon.getVisibility());

        mTrailingIcon.setVisibility(View.VISIBLE);
        mNtpThemeListItemView.setTrailingIconVisibility(false);
        assertEquals(View.INVISIBLE, mTrailingIcon.getVisibility());
    }
}
