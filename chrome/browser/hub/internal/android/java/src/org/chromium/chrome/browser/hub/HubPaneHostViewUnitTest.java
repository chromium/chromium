// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.app.Activity;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link HubPaneHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneHostViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<ViewGroup> mSnackbarContainerCallback;
    @Mock private HubColorMixer mColorMixer;

    private final ActivityController<TestActivity> mActivityController =
            Robolectric.buildActivity(TestActivity.class).setup();
    private final Activity mActivity = mActivityController.get();

    @Spy
    private HubPaneHostView mPaneHost =
            (HubPaneHostView)
                    LayoutInflater.from(mActivity)
                            .inflate(R.layout.hub_pane_host_layout, null, false);

    private ViewGroup mSnackbarContainer;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mSnackbarContainer = mPaneHost.findViewById(R.id.pane_host_view_snackbar_container);
        mActivity.setContentView(mPaneHost);

        // Explicitly set layout parameters and force a layout pass.
        mPaneHost.setLayoutParams(new FrameLayout.LayoutParams(1000, 1000));
        mPaneHost.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mPaneHost.layout(0, 0, 1000, 1000);

        mPropertyModel =
                new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS)
                        .with(COLOR_MIXER, mColorMixer)
                        .build();
        PropertyModelChangeProcessor.create(mPropertyModel, mPaneHost, HubPaneHostViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testSetRootView() {
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(200, 300);
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);
        View root3 = new View(mActivity);

        ViewGroup paneFrame = mPaneHost.findViewById(R.id.pane_frame);
        paneFrame.setLayoutParams(layoutParams);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(0, paneFrame.getChildCount());

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        verifyChildren(paneFrame, root1);

        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root1, root2);

        RobolectricUtil.runAllBackgroundAndUi();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        mPropertyModel.set(PANE_ROOT_VIEW, root3);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root2, root3);

        RobolectricUtil.runAllBackgroundAndUi();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, null);
        assertEquals(0, paneFrame.getChildCount());
    }

    @Test
    public void testSetRootView_alphaRestored() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, root2.getAlpha(), /* delta= */ 0);

        // Inspired by b/325372945 where the alpha needed to be reset, even when no animations ran.
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        assertEquals(1, root1.getAlpha(), /* delta= */ 0);
    }

    @Test
    public void testSetRootView_translationRestored() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(0, root2.getTranslationX(), /* delta= */ 0);

        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        assertEquals(0, root1.getTranslationX(), /* delta= */ 0);
    }

    @Test
    public void testSnackbarContainerSupplier() {
        mPropertyModel.set(SNACKBAR_CONTAINER_CALLBACK, mSnackbarContainerCallback);
        verify(mSnackbarContainerCallback).onResult(mSnackbarContainer);
    }

    @Test
    public void testHubColorScheme() {
        verify(mColorMixer, times(1)).registerBlend(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE)
    public void testTap_performClick() {
        int viewWidth = mPaneHost.getWidth();
        int viewHeight = mPaneHost.getHeight();
        long downTime = SystemClock.uptimeMillis();

        mPaneHost.onTouchEvent(
                MotionEvent.obtain(
                        downTime,
                        downTime,
                        MotionEvent.ACTION_DOWN,
                        viewWidth / 2f,
                        viewHeight / 2f,
                        0));
        mPaneHost.onTouchEvent(
                MotionEvent.obtain(
                        downTime,
                        downTime + 10,
                        MotionEvent.ACTION_UP,
                        viewWidth / 2f,
                        viewHeight / 2f,
                        0));

        verify(mPaneHost).performClick();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE)
    public void testTap_unhandled_doesNotPerformClick() {
        int viewWidth = mPaneHost.getWidth();
        int viewHeight = mPaneHost.getHeight();
        long downTime = SystemClock.uptimeMillis();

        mPaneHost.onTouchEvent(
                MotionEvent.obtain(
                        downTime,
                        downTime,
                        MotionEvent.ACTION_DOWN,
                        viewWidth / 2f,
                        viewHeight / 2f,
                        0));
        mPaneHost.onTouchEvent(
                MotionEvent.obtain(
                        downTime,
                        downTime + 10,
                        MotionEvent.ACTION_UP,
                        viewWidth / 2f,
                        viewHeight / 2f,
                        0));

        verify(mPaneHost, never()).performClick();
    }

    /** Order of children does not matter. */
    private void verifyChildren(ViewGroup parent, View... children) {
        assertEquals(children.length, parent.getChildCount());
        List<View> expectedChildList = Arrays.asList(children);
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            assertTrue(child.toString(), expectedChildList.contains(child));
        }
    }
}
