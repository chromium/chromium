// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.widget.FrameLayout;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;

/**
 * Unit tests for ToggleTabStackButton.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ToggleTabStackButtonTest {
    @Mock
    private Drawable mDrawable;

    private ToggleTabStackButton mToggleTabStackButton;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        FrameLayout content = new FrameLayout(mActivity);
        mActivity.setContentView(content);
        mToggleTabStackButton = new ToggleTabStackButton(mActivity, null);
    }

    @Test
    public void testHighlight() {
        mToggleTabStackButton.setBackground(mDrawable);

        mToggleTabStackButton.setHighlightDrawable(true);
        Assert.assertNotEquals("Should not use original drawable when highlighted", mDrawable,
                mToggleTabStackButton.getBackground());
        Assert.assertThat("Should use PulseDrawable when highlighted",
                mToggleTabStackButton.getBackground(), Matchers.instanceOf(PulseDrawable.class));

        mToggleTabStackButton.setHighlightDrawable(false);
        Assert.assertEquals("Should be using original drawable when not highlighted", mDrawable,
                mToggleTabStackButton.getBackground());

        mToggleTabStackButton.setHighlightDrawable(true);
        Assert.assertNotEquals("Should not use original drawable when highlighted", mDrawable,
                mToggleTabStackButton.getBackground());
        Assert.assertThat("Should use PulseDrawable when highlighted",
                mToggleTabStackButton.getBackground(), Matchers.instanceOf(PulseDrawable.class));
    }
}