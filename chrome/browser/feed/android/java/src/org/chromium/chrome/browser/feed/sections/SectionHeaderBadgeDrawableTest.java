// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.app.Activity;
import android.graphics.Rect;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.test.R;

/** Tests for the {@link SectionHeaderBadgeDrawabe} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class SectionHeaderBadgeDrawableTest {
    private Activity mActivity;
    private SectionHeaderBadgeDrawable mDrawable;

    @Mock LinearLayout mMockAnchor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_MaterialComponents);

        mDrawable = new SectionHeaderBadgeDrawable(mActivity);
    }

    @Test
    public void pendingAnimationWhenNonNullTextAndNotAttached() {
        mDrawable.setText("new");
        mDrawable.startAnimation();
        assertTrue(mDrawable.getHasPendingAnimationForTest());
    }

    @Test
    public void pendingAnimationStartsFalse() {
        assertFalse(mDrawable.getHasPendingAnimationForTest());
    }

    @Test
    public void animationStartsWhenNonNullTextAndAttached() {
        mDrawable.setText("new");
        doAnswer(
                        (invocation) -> {
                            Rect r = invocation.getArgument(0);
                            r.left = 0;
                            r.top = 0;
                            r.right = 300;
                            r.bottom = 200;
                            return null;
                        })
                .when(mMockAnchor)
                .getDrawingRect(any());
        mDrawable.attach(new LinearLayout(mActivity));
        mDrawable.startAnimation();
        assertFalse(mDrawable.getHasPendingAnimationForTest());
        assertTrue(mDrawable.getAnimatorForTest().isStarted());
    }
}
