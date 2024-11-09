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
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** On-device unit tests for {@link MinimizedCardCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MinimizedCardCoordinatorTest {
    private static final String TITLE = "Google";
    private static final String URL = "google.com";

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private MinimizedCardCoordinator mCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = ThreadUtils.runOnUiThreadBlocking(() -> sActivityTestRule.getActivity());
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var layoutParams =
                            new FrameLayout.LayoutParams(
                                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                    ViewGroup content = new FrameLayout(sActivity);
                    sActivity.setContentView(content, layoutParams);
                    CoordinatorLayout coordinator = new CoordinatorLayout(sActivity);
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
                    mCoordinator = new MinimizedCardCoordinator(sActivity, content, model);
                });
    }

    @Test
    @SmallTest
    public void testConstructAndDestroy() {
        onView(withId(R.id.card)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                            sActivity
                                    .findViewById(R.id.coordinator)
                                    .getImportantForAccessibility());
                });

        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.dismiss());

        onView(withId(R.id.card)).check(doesNotExist());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                            sActivity
                                    .findViewById(R.id.coordinator)
                                    .getImportantForAccessibility());
                });
    }
}
