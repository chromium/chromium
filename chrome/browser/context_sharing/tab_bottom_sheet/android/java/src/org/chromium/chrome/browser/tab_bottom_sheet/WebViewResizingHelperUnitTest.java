// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.thinwebview.ThinWebView;

/** Unit tests for {@link WebViewResizingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebViewResizingHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ThinWebView mMockThinWebView;

    private Context mContext;
    private View mView;
    private WebViewResizingHelper mHelper;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView = new View(mContext);
        when(mMockThinWebView.getView()).thenReturn(mView);

        mHelper = new WebViewResizingHelper(mContext);
    }

    @Test
    public void testInitialization() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(0, container.getChildCount());
    }

    @Test
    public void testSetThinWebView() {
        mHelper.setThinWebView(mMockThinWebView);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(mView, container.getChildAt(0));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(Gravity.BOTTOM, layoutParams.gravity);
    }

    @Test
    public void testReset() {
        mHelper.setThinWebView(mMockThinWebView);
        mHelper.reset();

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(0, container.getChildCount());
    }

    @Test
    public void testSetIsResizing() {
        mHelper.setThinWebView(mMockThinWebView);
        mView.layout(0, 0, 100, 200);

        mHelper.setIsResizing(true);
        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(200, layoutParams.height);

        mHelper.setIsResizing(false);
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, layoutParams.height);
    }

    @Test
    public void testSetThinWebViewMultipleTimes() {
        mHelper.setThinWebView(mMockThinWebView);
        mHelper.setThinWebView(mMockThinWebView);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(mView, container.getChildAt(0));
    }
}
