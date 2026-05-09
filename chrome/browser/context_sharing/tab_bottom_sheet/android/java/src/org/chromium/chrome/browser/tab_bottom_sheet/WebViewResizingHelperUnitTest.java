// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link WebViewResizingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebViewResizingHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ThinWebView mMockThinWebView;
    @Mock private WebContents mMockWebContents;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;

    private Context mContext;
    private View mView;
    private View mContainerView;
    private WebViewResizingHelper mHelper;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
        mView = new View(mContext);
        when(mMockThinWebView.getView()).thenReturn(mView);

        when(mMockWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockDecorView.getHeight()).thenReturn(1000);

        mContainerView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mHelper = new WebViewResizingHelper(mContainerView, mMockWindowAndroid, Color.WHITE);
    }

    @Test
    public void testInitialization() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
    }

    @Test
    public void testSetThinWebView() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(2, container.getChildCount());
        assertEquals(mView, container.getChildAt(1));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(Gravity.TOP, layoutParams.gravity);
    }

    @Test
    public void testReset() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mHelper.reset();

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
    }

    @Test
    public void testRequestResize() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        View placeholder = container.getChildAt(0);

        WebViewResizingHelper.ResizeLock lock = mHelper.requestResize();
        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();

        assertEquals(1000, layoutParams.height);
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        lock.unlock();
        assertEquals(1000, layoutParams.height);
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void testSetThinWebViewMultipleTimes() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(2, container.getChildCount());
        assertEquals(mView, container.getChildAt(1));
    }

    @Test
    public void testSetToFlexibleHeight() {
        View expandedContent = mContainerView.findViewById(R.id.expanded_content_group);
        expandedContent.getLayoutParams().height = 500;

        mHelper.setToFlexibleHeight();
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, expandedContent.getLayoutParams().height);
    }

    @Test
    public void testSetToFixedHeight() {
        View expandedContent = mContainerView.findViewById(R.id.expanded_content_group);

        mHelper.setToFixedHeight(500);
        assertEquals(500, expandedContent.getLayoutParams().height);
    }
}
