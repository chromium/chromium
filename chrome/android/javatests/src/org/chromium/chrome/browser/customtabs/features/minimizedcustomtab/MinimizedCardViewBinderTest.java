// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManager.ASPECT_RATIO;

import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** On-device unit tests for {@link MinimizedCardViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MinimizedCardViewBinderTest extends BlankUiTestActivityTestCase {
    private static final int HEIGHT_DP = 90;
    private static final String SHORT_TITLE = "Google";
    private static final String LONG_TITLE =
            "Very Long Title of a Website That You Would Have Come Across on the Interweb";
    private static final String SHORT_URL = "google.com";
    private static final String LONG_URL =
            "subdomain.longlonglonglonglonglonglonglong.awebsitewithalongurl.com";

    private View mView;
    private PropertyModel mModel;
    private TextView mTitle;
    private TextView mUrl;
    private ImageView mFavicon;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    float density = getActivity().getResources().getDisplayMetrics().density;
                    int height = Math.round(HEIGHT_DP * density);
                    int width = Math.round(ASPECT_RATIO.floatValue() * height);
                    var layoutParams = new FrameLayout.LayoutParams(width, height);
                    ViewGroup content = new FrameLayout(getActivity());
                    getActivity().setContentView(content, layoutParams);
                    mView =
                            LayoutInflater.from(getActivity())
                                    .inflate(R.layout.custom_tabs_minimized_card, content, true);
                    mModel =
                            new PropertyModel.Builder(MinimizedCardProperties.ALL_KEYS)
                                    .with(MinimizedCardProperties.TITLE, "")
                                    .with(MinimizedCardProperties.URL, "")
                                    .with(MinimizedCardProperties.FAVICON, null)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, MinimizedCardViewBinder::bind);
                    mTitle = mView.findViewById(R.id.title);
                    mUrl = mView.findViewById(R.id.url);
                    mFavicon = mView.findViewById(R.id.favicon);
                });
    }

    @Test
    @SmallTest
    public void testTitleUrlFavicon() {
        var favicon = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(MinimizedCardProperties.TITLE, SHORT_TITLE);
                    mModel.set(MinimizedCardProperties.URL, SHORT_URL);
                    mModel.set(MinimizedCardProperties.FAVICON, favicon);
                });

        onView(withId(R.id.title)).check(matches(withText(SHORT_TITLE)));
        onView(withId(R.id.url)).check(matches(withText(SHORT_URL)));
        onView(withId(R.id.favicon)).check(matches(isCompletelyDisplayed()));
        assertEquals(favicon, ((RoundedBitmapDrawable) mFavicon.getDrawable()).getBitmap());
    }

    @Test
    @SmallTest
    public void testTitleUrlFaviconLong() {
        var favicon = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(MinimizedCardProperties.TITLE, LONG_TITLE);
                    mModel.set(MinimizedCardProperties.URL, LONG_URL);
                    mModel.set(MinimizedCardProperties.FAVICON, favicon);
                });

        onView(withId(R.id.title)).check(matches(withText(LONG_TITLE)));
        assertEquals(1, mTitle.getLineCount());
        onView(withId(R.id.url)).check(matches(withText(LONG_URL)));
        assertEquals(1, mUrl.getLineCount());
        onView(withId(R.id.favicon)).check(matches(isCompletelyDisplayed()));
        assertEquals(favicon, ((RoundedBitmapDrawable) mFavicon.getDrawable()).getBitmap());
    }

    @Test
    @SmallTest
    public void testEmptyTitle() {
        var favicon = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(MinimizedCardProperties.URL, SHORT_URL);
                    mModel.set(MinimizedCardProperties.FAVICON, favicon);
                });

        onView(withId(R.id.title)).check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(withId(R.id.url)).check(matches(withText(SHORT_URL)));
        assertEquals(1, mUrl.getLineCount());
        onView(withId(R.id.favicon)).check(matches(isCompletelyDisplayed()));
        assertEquals(favicon, ((RoundedBitmapDrawable) mFavicon.getDrawable()).getBitmap());
    }

    @Test
    @SmallTest
    public void testNullFavicon() {
        onView(withId(R.id.favicon)).check(matches(isCompletelyDisplayed()));
        assertNotNull(mFavicon.getDrawable());
    }
}
