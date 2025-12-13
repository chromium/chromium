// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link DesktopPopupHeaderViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DesktopPopupHeaderViewBinderUnitTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private View mHeaderView;
    private TextView mTitleView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Manually construct the view hierarchy.
                    LinearLayout root = new LinearLayout(mActivity);
                    root.setLayoutParams(
                            new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));

                    mTitleView = new TextView(mActivity);
                    mTitleView.setId(R.id.desktop_popup_header_text_view);
                    root.addView(
                            mTitleView,
                            new LinearLayout.LayoutParams(
                                    ViewGroup.LayoutParams.WRAP_CONTENT, 0, 1));

                    mHeaderView = root;

                    mModel =
                            new PropertyModel.Builder(DesktopPopupHeaderProperties.ALL_KEYS)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            mModel, mHeaderView, DesktopPopupHeaderViewBinder::bind);
                });
    }

    @Test
    @SmallTest
    public void testIsShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(DesktopPopupHeaderProperties.IS_SHOWN, true);
                    assertEquals(View.VISIBLE, mHeaderView.getVisibility());

                    mModel.set(DesktopPopupHeaderProperties.IS_SHOWN, false);
                    assertEquals(View.GONE, mHeaderView.getVisibility());
                });
    }

    @Test
    @SmallTest
    public void testTitleText() {
        String title = "My Amazing Popup";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(DesktopPopupHeaderProperties.TITLE_TEXT, title);
                    assertEquals(title, mTitleView.getText().toString());
                });
    }

    @Test
    @SmallTest
    public void testTitleShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(DesktopPopupHeaderProperties.TITLE_VISIBLE, true);
                    assertEquals(View.VISIBLE, mTitleView.getVisibility());

                    mModel.set(DesktopPopupHeaderProperties.TITLE_VISIBLE, false);
                    assertEquals(View.GONE, mTitleView.getVisibility());
                });
    }

    @Test
    @SmallTest
    public void testTitleAppearance() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Just testing that it doesn't crash and hopefully sets something.
                    mModel.set(
                            DesktopPopupHeaderProperties.TITLE_APPEARANCE,
                            android.R.style.TextAppearance_Large);
                });
    }

    @Test
    @SmallTest
    public void testTitleHorizontalMargins() {
        int leftMargin = 10;
        int topMargin = 30;
        int rightMargin = 20;
        int bottomMargin = 60;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            DesktopPopupHeaderProperties.TITLE_SPACING,
                            Insets.of(leftMargin, topMargin, rightMargin, bottomMargin));

                    LinearLayout.LayoutParams params =
                            (LinearLayout.LayoutParams) mTitleView.getLayoutParams();
                    assertEquals(leftMargin, params.leftMargin);
                    assertEquals(topMargin, params.topMargin);
                    assertEquals(rightMargin, params.rightMargin);
                    assertEquals(bottomMargin, params.bottomMargin);
                });
    }

    @Test
    @SmallTest
    public void testBackgroundColor() {
        int color = Color.RED;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(DesktopPopupHeaderProperties.BACKGROUND_COLOR, color);

                    ColorDrawable background = (ColorDrawable) mHeaderView.getBackground();
                    assertNotNull(background);
                    assertEquals(color, background.getColor());
                });
    }

    @Test
    @SmallTest
    public void testHeaderHeight() {
        int height = 123;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX, height);

                    assertEquals(height, mHeaderView.getLayoutParams().height);
                });
    }
}
