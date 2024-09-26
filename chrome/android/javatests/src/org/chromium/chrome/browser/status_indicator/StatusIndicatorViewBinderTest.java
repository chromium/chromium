// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static android.graphics.PorterDuff.Mode.SRC_IN;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link StatusIndicatorViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class StatusIndicatorViewBinderTest extends BlankUiTestActivityTestCase {
    private static final String STATUS_TEXT = "Offline";

    private ViewResourceFrameLayout mContainer;
    private TextViewWithCompoundDrawables mStatusTextView;
    private MockStatusIndicatorSceneLayer mSceneLayer;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(R.layout.status_indicator_container);
                    mContainer = getActivity().findViewById(R.id.status_indicator);
                    mStatusTextView = mContainer.findViewById(R.id.status_text);

                    mSceneLayer = new MockStatusIndicatorSceneLayer();
                    mModel =
                            new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                                    .with(StatusIndicatorProperties.STATUS_TEXT, "")
                                    .with(StatusIndicatorProperties.STATUS_ICON, null)
                                    .with(
                                            StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY,
                                            View.GONE)
                                    .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                                    .build();
                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel,
                                    new StatusIndicatorViewBinder.ViewHolder(
                                            mContainer, mSceneLayer),
                                    StatusIndicatorViewBinder::bind);
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTextView() {
        assertTrue("Wrong initial status text.", TextUtils.isEmpty(mStatusTextView.getText()));
        assertNull("Wrong initial status icon.", mStatusTextView.getCompoundDrawablesRelative()[0]);
        assertTrue(
                "Rest of the compound drawables are not null.", areRestOfCompoundDrawablesNull());

        Drawable drawable =
                ResourcesCompat.getDrawable(
                        getActivity().getResources(),
                        R.drawable.ic_error_white_24dp_filled,
                        getActivity().getTheme());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(StatusIndicatorProperties.STATUS_TEXT, STATUS_TEXT);
                    mModel.set(StatusIndicatorProperties.STATUS_ICON, drawable);
                });

        assertEquals("Wrong status text.", STATUS_TEXT, mStatusTextView.getText());
        assertEquals(
                "Wrong status icon.", drawable, mStatusTextView.getCompoundDrawablesRelative()[0]);
        assertTrue(
                "Rest of the compound drawables are not null.", areRestOfCompoundDrawablesNull());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testVisibility() {
        assertEquals(
                "Wrong initial Android view visibility.", View.GONE, mContainer.getVisibility());
        assertFalse(
                "Wrong initial composited view visibility.",
                mSceneLayer.isSceneOverlayTreeShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.VISIBLE);
                    mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, true);
                });

        assertEquals("Android view is not visible.", View.VISIBLE, mContainer.getVisibility());
        assertTrue("Composited view is not visible.", mSceneLayer.isSceneOverlayTreeShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.GONE);
                    mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false);
                });

        assertEquals("Android view is not gone.", View.GONE, mContainer.getVisibility());
        assertFalse("Composited view is visible.", mSceneLayer.isSceneOverlayTreeShowing());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testColorAndTint() {
        int bgColor = SemanticColorUtils.getDefaultBgColor(getActivity());
        int textColor = SemanticColorUtils.getDefaultTextColor(getActivity());
        assertEquals(
                "Wrong initial background color.",
                bgColor,
                ((ColorDrawable) mContainer.getBackground()).getColor());
        assertEquals("Wrong initial text color", textColor, mStatusTextView.getCurrentTextColor());

        Drawable drawable =
                ResourcesCompat.getDrawable(
                        getActivity().getResources(),
                        R.drawable.ic_error_white_24dp_filled,
                        getActivity().getTheme());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(StatusIndicatorProperties.STATUS_ICON, drawable);
                    mModel.set(StatusIndicatorProperties.BACKGROUND_COLOR, Color.BLUE);
                    mModel.set(StatusIndicatorProperties.TEXT_COLOR, Color.RED);
                    mModel.set(StatusIndicatorProperties.ICON_TINT, Color.GREEN);
                });

        assertEquals(
                "Wrong background color.",
                Color.BLUE,
                ((ColorDrawable) mContainer.getBackground()).getColor());
        assertEquals("Wrong text color.", Color.RED, mStatusTextView.getCurrentTextColor());
        assertEquals(
                "Wrong compound drawables tint",
                new PorterDuffColorFilter(Color.GREEN, SRC_IN),
                mStatusTextView.getCompoundDrawablesRelative()[0].getColorFilter());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTextAlpha() {
        assertEquals(
                "Wrong initial text alpha.", 1.f, mStatusTextView.getAlpha(), MathUtils.EPSILON);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(StatusIndicatorProperties.TEXT_ALPHA, .5f));

        assertEquals("Wrong text alpha.", .5f, mStatusTextView.getAlpha(), MathUtils.EPSILON);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(StatusIndicatorProperties.TEXT_ALPHA, .0f));

        assertEquals("Wrong text alpha.", 0.f, mStatusTextView.getAlpha(), MathUtils.EPSILON);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(StatusIndicatorProperties.TEXT_ALPHA, 1.f));

        assertEquals("Wrong text alpha.", 1.f, mStatusTextView.getAlpha(), MathUtils.EPSILON);
    }

    private boolean areRestOfCompoundDrawablesNull() {
        final Drawable[] drawables = mStatusTextView.getCompoundDrawablesRelative();
        for (int i = 1; i < drawables.length; i++) {
            if (drawables[i] != null) {
                return false;
            }
        }

        return true;
    }

    /** Mock {@link StatusIndicatorSceneLayer} class to avoid native initialization. */
    private static class MockStatusIndicatorSceneLayer extends StatusIndicatorSceneLayer {
        MockStatusIndicatorSceneLayer() {
            super(null);
        }

        @Override
        protected void initializeNative() {}

        @Override
        public void destroy() {}
    }
}
