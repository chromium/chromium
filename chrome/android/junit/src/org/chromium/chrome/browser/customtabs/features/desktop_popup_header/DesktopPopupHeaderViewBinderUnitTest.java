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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link DesktopPopupHeaderViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DesktopPopupHeaderViewBinderUnitTest {
    private Activity mActivity;
    private View mHeaderView;
    private TextView mTitleView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        // Manually construct the view hierarchy.
        LinearLayout root = new LinearLayout(mActivity);
        root.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));

        mTitleView = new TextView(mActivity);
        mTitleView.setId(R.id.desktop_popup_header_text_view);
        root.addView(
                mTitleView,
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, 0, 1));

        mHeaderView = root;

        mModel = new PropertyModel.Builder(DesktopPopupHeaderProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel, mHeaderView, DesktopPopupHeaderViewBinder::bind);
    }

    @Test
    public void testIsShown() {

        mModel.set(DesktopPopupHeaderProperties.IS_SHOWN, true);
        assertEquals(View.VISIBLE, mHeaderView.getVisibility());

        mModel.set(DesktopPopupHeaderProperties.IS_SHOWN, false);
        assertEquals(View.GONE, mHeaderView.getVisibility());
    }

    @Test
    public void testTitleText() {
        String title = "My Amazing Popup";

        mModel.set(DesktopPopupHeaderProperties.TITLE_TEXT, title);
        assertEquals(title, mTitleView.getText().toString());
    }

    @Test
    public void testTitleShown() {

        mModel.set(DesktopPopupHeaderProperties.TITLE_VISIBLE, true);
        assertEquals(View.VISIBLE, mTitleView.getVisibility());

        mModel.set(DesktopPopupHeaderProperties.TITLE_VISIBLE, false);
        assertEquals(View.GONE, mTitleView.getVisibility());
    }

    @Test
    public void testTitleAppearance() {

        // Just testing that it doesn't crash and hopefully sets something.
        mModel.set(
                DesktopPopupHeaderProperties.TITLE_APPEARANCE,
                android.R.style.TextAppearance_Large);
    }

    @Test
    public void testTitleHorizontalMargins() {
        int leftMargin = 10;
        int topMargin = 30;
        int rightMargin = 20;
        int bottomMargin = 60;

        mModel.set(
                DesktopPopupHeaderProperties.TITLE_SPACING,
                Insets.of(leftMargin, topMargin, rightMargin, bottomMargin));

        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) mTitleView.getLayoutParams();
        assertEquals(leftMargin, params.leftMargin);
        assertEquals(topMargin, params.topMargin);
        assertEquals(rightMargin, params.rightMargin);
        assertEquals(bottomMargin, params.bottomMargin);
    }

    @Test
    public void testBackgroundColor() {
        int color = Color.RED;

        mModel.set(DesktopPopupHeaderProperties.BACKGROUND_COLOR, color);

        ColorDrawable background = (ColorDrawable) mHeaderView.getBackground();
        assertNotNull(background);
        assertEquals(color, background.getColor());
    }

    @Test
    public void testHeaderHeight() {
        int height = 123;

        mModel.set(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX, height);

        assertEquals(height, mHeaderView.getLayoutParams().height);
    }
}
