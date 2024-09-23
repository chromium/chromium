// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.display.DisplayAndroid;

/** Tests anchor views are correctly added/removed when their container view is updated. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsAnchorViewTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private FrameLayout mContainerView;
    private AwViewAndroidDelegate mViewDelegate;

    public AwContentsAnchorViewTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView = new FrameLayout(mActivityTestRule.getActivity());
                    mViewDelegate = new AwViewAndroidDelegate(mContainerView, null, null);
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAddAndRemoveAnchorViews() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add 2 anchor views
                    View anchorView1 = addAnchorView();
                    View anchorView2 = addAnchorView();

                    // Remove anchorView1
                    removeAnchorView(anchorView1);

                    // Try to remove anchorView1 again; no-op.
                    removeAnchorView(anchorView1);

                    // Remove anchorView2
                    removeAnchorView(anchorView2);
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAddAndMoveAnchorView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add anchorView and set layout params
                    View anchorView = addAnchorView();
                    LayoutParams originalLayoutParams = setLayoutParams(anchorView, 0, 0);

                    // Move it
                    LayoutParams updatedLayoutParams = setLayoutParams(anchorView, 1, 2);
                    Assert.assertFalse(areEqual(originalLayoutParams, updatedLayoutParams));

                    // Move it back to the original position
                    updatedLayoutParams = setLayoutParams(anchorView, 0, 0);
                    Assert.assertTrue(areEqual(originalLayoutParams, updatedLayoutParams));
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMovedAndRemovedAnchorViewIsNotTransferred() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add, move and remove anchorView
                    View anchorView = addAnchorView();
                    setLayoutParams(anchorView, 1, 2);
                    removeAnchorView(anchorView);

                    // Replace container view
                    FrameLayout updatedContainerView = updateContainerView();

                    // Verify that no anchor view is transferred between containerViews
                    Assert.assertFalse(isViewInContainer(mContainerView, anchorView));
                    Assert.assertFalse(isViewInContainer(updatedContainerView, anchorView));
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testTransferAnchorView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Add anchor view
                    View anchorView = addAnchorView();
                    LayoutParams layoutParams = new LayoutParams(anchorView.getLayoutParams());

                    // Replace container view
                    FrameLayout updatedContainerView = updateContainerView();
                    verifyAnchorViewCorrectlyTransferred(
                            mContainerView, anchorView, updatedContainerView, layoutParams);
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testTransferMovedAnchorView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Add anchor view and move it
                    View anchorView = addAnchorView();
                    LayoutParams layoutParams = setLayoutParams(anchorView, 1, 2);

                    // Replace container view
                    FrameLayout updatedContainerView = updateContainerView();

                    verifyAnchorViewCorrectlyTransferred(
                            mContainerView, anchorView, updatedContainerView, layoutParams);
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testRemoveTransferedAnchorView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Add anchor view
                    View anchorView = addAnchorView();

                    // Replace container view
                    FrameLayout updatedContainerView = updateContainerView();

                    verifyAnchorViewCorrectlyTransferred(
                            mContainerView, anchorView, updatedContainerView);

                    // Remove transferred anchor view
                    removeAnchorView(anchorView);
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMoveTransferedAnchorView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Add anchor view
                    View anchorView = addAnchorView();
                    LayoutParams layoutParams = new LayoutParams(anchorView.getLayoutParams());

                    // Replace container view
                    FrameLayout updatedContainerView = updateContainerView();

                    verifyAnchorViewCorrectlyTransferred(
                            mContainerView, anchorView, updatedContainerView, layoutParams);

                    // Move transferred anchor view
                    Assert.assertFalse(areEqual(layoutParams, setLayoutParams(anchorView, 1, 2)));
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testTransferMultipleMovedAnchorViews() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Add and move anchorView1
                    View anchorView1 = addAnchorView();
                    LayoutParams layoutParams1 = setLayoutParams(anchorView1, 1, 2);

                    // Add and move anchorView2
                    View anchorView2 = addAnchorView();
                    LayoutParams layoutParams2 = setLayoutParams(anchorView2, 2, 4);

                    // Replace containerView
                    FrameLayout updatedContainerView = updateContainerView();

                    // Verify that anchor views are transferred with the same layout params.
                    Assert.assertFalse(isViewInContainer(mContainerView, anchorView1));
                    Assert.assertFalse(isViewInContainer(mContainerView, anchorView2));
                    Assert.assertTrue(isViewInContainer(updatedContainerView, anchorView1));
                    Assert.assertTrue(isViewInContainer(updatedContainerView, anchorView2));
                    Assert.assertTrue(areEqual(layoutParams1, anchorView1.getLayoutParams()));
                    Assert.assertTrue(areEqual(layoutParams2, anchorView2.getLayoutParams()));
                });
    }

    private View addAnchorView() {
        View anchorView = mViewDelegate.acquireView();
        Assert.assertTrue(isViewInContainer(mContainerView, anchorView));
        return anchorView;
    }

    private void removeAnchorView(View anchorView) {
        mViewDelegate.removeView(anchorView);
        Assert.assertFalse(isViewInContainer(mContainerView, anchorView));
    }

    private LayoutParams setLayoutParams(View anchorView, int coords, int dimension) {
        float scale = DisplayAndroid.getNonMultiDisplay(mContainerView.getContext()).getDipScale();
        float scaledCoords = (float) coords * scale;
        float scaledDimension = (float) dimension * scale;
        mViewDelegate.setViewPosition(
                anchorView, scaledCoords, scaledCoords, scaledDimension, scaledDimension, 10, 10);
        return new LayoutParams(anchorView.getLayoutParams());
    }

    private FrameLayout updateContainerView() {
        FrameLayout containerView = new FrameLayout(mActivityTestRule.getActivity());
        mActivityTestRule.getActivity().addView(containerView);
        mViewDelegate.setContainerView(containerView);
        return containerView;
    }

    private static void verifyAnchorViewCorrectlyTransferred(
            FrameLayout containerView,
            View anchorView,
            FrameLayout updatedContainerView,
            LayoutParams expectedParams) {
        Assert.assertTrue(areEqual(expectedParams, anchorView.getLayoutParams()));
        verifyAnchorViewCorrectlyTransferred(containerView, anchorView, updatedContainerView);
    }

    private static void verifyAnchorViewCorrectlyTransferred(
            FrameLayout containerView, View anchorView, FrameLayout updatedContainerView) {
        Assert.assertFalse(isViewInContainer(containerView, anchorView));
        Assert.assertTrue(isViewInContainer(updatedContainerView, anchorView));
        Assert.assertSame(anchorView, updatedContainerView.getChildAt(0));
    }

    private static boolean areEqual(LayoutParams params1, LayoutParams params2) {
        return params1.height == params2.height && params1.width == params2.width;
    }

    private static boolean isViewInContainer(FrameLayout containerView, View view) {
        return containerView.indexOfChild(view) != -1;
    }
}
