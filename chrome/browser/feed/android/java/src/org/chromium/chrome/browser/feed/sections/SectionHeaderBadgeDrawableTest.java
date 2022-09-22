// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class SectionHeaderBadgeDrawableTest {
    private Activity mActivity;
    private SectionHeaderBadgeDrawable mDrawable;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_MaterialComponents);

        mDrawable = new SectionHeaderBadgeDrawable(mActivity);
    }

    @Test
    public void pendingAnimationWhenNonNullTextAndNotAttached() {
        mDrawable.setText("new");
        assertTrue(mDrawable.getHasPendingAnimationForTest());
    }

    @Test
    public void pendingAnimationStartsFalse() {
        assertFalse(mDrawable.getHasPendingAnimationForTest());
    }

    @Test
    public void pendingAnimationFalseForNoTextChange() {
        mDrawable.setText(null);
        assertFalse(mDrawable.getHasPendingAnimationForTest());
    }

    @Test
    public void pendingAnimationFalseForNullText() {
        mDrawable.setText("new");
        mDrawable.setText(null);
        assertFalse(mDrawable.getHasPendingAnimationForTest());
    }
}
