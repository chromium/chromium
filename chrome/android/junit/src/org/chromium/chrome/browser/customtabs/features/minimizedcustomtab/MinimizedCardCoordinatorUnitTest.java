// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MinimizedCardCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MinimizedCardCoordinatorUnitTest {
    private static final String TITLE = "Google";
    private static final String URL = "google.com";

    private Activity mActivity;

    private MinimizedCardCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        var layoutParams =
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        ViewGroup content = new FrameLayout(mActivity);
        mActivity.setContentView(content, layoutParams);
        CoordinatorLayout coordinator = new CoordinatorLayout(mActivity);
        coordinator.setId(R.id.coordinator);
        coordinator.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        content.addView(coordinator);

        var favicon = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        PropertyModel model =
                new PropertyModel.Builder(MinimizedCardProperties.ALL_KEYS)
                        .with(MinimizedCardProperties.TITLE, TITLE)
                        .with(MinimizedCardProperties.URL, URL)
                        .with(MinimizedCardProperties.FAVICON, favicon)
                        .build();
        mCoordinator = new MinimizedCardCoordinator(mActivity, content, model);
    }

    @Test
    public void testConstructAndDestroy() {
        onView(withId(R.id.card)).check(matches(isDisplayed()));

        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                mActivity.findViewById(R.id.coordinator).getImportantForAccessibility());

        mCoordinator.dismiss();

        onView(withId(R.id.card)).check(doesNotExist());

        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                mActivity.findViewById(R.id.coordinator).getImportantForAccessibility());
    }
}
