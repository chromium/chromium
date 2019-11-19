// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import android.graphics.drawable.Drawable;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.v4.content.res.ResourcesCompat;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.ViewResourceFrameLayout;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link StatusIndicatorViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class StatusIndicatorViewBinderTest extends DummyUiActivityTestCase {
    private static final String STATUS_TEXT = "Offline";

    private ViewResourceFrameLayout mContainer;
    private TextView mStatusTextView;
    private MockStatusIndicatorSceneLayer mSceneLayer;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.status_indicator_container);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainer = getActivity().findViewById(R.id.status_indicator);
            mStatusTextView = mContainer.findViewById(R.id.status_text);
        });
        mSceneLayer = new MockStatusIndicatorSceneLayer(mContainer);
        mModel = new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                         .with(StatusIndicatorProperties.STATUS_TEXT, "")
                         .with(StatusIndicatorProperties.STATUS_ICON, null)
                         .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, false)
                         .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                         .build();
        mMCP = PropertyModelChangeProcessor.create(mModel,
                new StatusIndicatorViewBinder.ViewHolder(mContainer, mSceneLayer),
                StatusIndicatorViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTextView() {
        Assert.assertTrue(
                "Wrong initial status text.", TextUtils.isEmpty(mStatusTextView.getText()));
        Assert.assertNull(
                "Wrong initial status icon.", mStatusTextView.getCompoundDrawablesRelative()[0]);
        Assert.assertTrue(
                "Rest of the compound drawables are not null.", areRestOfCompoundDrawablesNull());

        Drawable drawable = ResourcesCompat.getDrawable(getActivity().getResources(),
                R.drawable.ic_error_white_24dp_filled, getActivity().getTheme());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(StatusIndicatorProperties.STATUS_TEXT, STATUS_TEXT);
            mModel.set(StatusIndicatorProperties.STATUS_ICON, drawable);
        });

        assertThat("Wrong status text.", mStatusTextView.getText(), equalTo(STATUS_TEXT));
        assertThat("Wrong status icon.", mStatusTextView.getCompoundDrawablesRelative()[0],
                equalTo(drawable));
        Assert.assertTrue(
                "Rest of the compound drawables are not null.", areRestOfCompoundDrawablesNull());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testVisibility() {
        assertThat("Wrong initial Android view visibility.", mContainer.getVisibility(),
                equalTo(View.GONE));
        Assert.assertFalse("Wrong initial composited view visibility.",
                mSceneLayer.isSceneOverlayTreeShowing());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, true);
            mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, true);
        });

        assertThat(
                "Android view is not visible.", mContainer.getVisibility(), equalTo(View.VISIBLE));
        Assert.assertTrue(
                "Composited view is not visible.", mSceneLayer.isSceneOverlayTreeShowing());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, false);
            mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false);
        });

        assertThat("Android view is not gone.", mContainer.getVisibility(), equalTo(View.GONE));
        Assert.assertFalse("Composited view is visible.", mSceneLayer.isSceneOverlayTreeShowing());
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
    private class MockStatusIndicatorSceneLayer extends StatusIndicatorSceneLayer {
        MockStatusIndicatorSceneLayer(ViewResourceFrameLayout statusIndicator) {
            super(statusIndicator);
        }

        @Override
        protected void initializeNative() {}

        @Override
        public void destroy() {}
    }
}
