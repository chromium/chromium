// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemViewBinder.SELECTION_LAYER;

import android.app.Activity;
import android.graphics.drawable.LayerDrawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/** Integration and render tests for the ColorPicker feature. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabGroupColorPickerTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(2)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private ColorPickerCoordinator mCoordinator;
    private ColorPickerContainer mContainerView;
    private FrameLayout mRootView;
    private List<Integer> mColorList;

    public TabGroupColorPickerTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int selectedIndex = 1;
                    int totalColorCount = 9;
                    Activity activity = mActivityTestRule.getActivity();

                    List<Integer> colors = new ArrayList<>();
                    for (int i = 0; i < totalColorCount; i++) {
                        colors.add(i);
                    }

                    mCoordinator =
                            new ColorPickerCoordinator(
                                    mActivityTestRule.getActivity(),
                                    colors,
                                    LayoutInflater.from(mActivityTestRule.getActivity())
                                            .inflate(
                                                    R.layout.tab_group_color_picker_container,
                                                    /* root= */ null),
                                    ColorPickerType.TAB_GROUP,
                                    false,
                                    ColorPickerLayoutType.DYNAMIC,
                                    null);
                    mCoordinator.setSelectedColorItem(colors.get(selectedIndex));
                    mContainerView = (ColorPickerContainer) mCoordinator.getContainerView();
                    mColorList = colors;

                    mRootView = new FrameLayout(activity);
                    activity.setContentView(mRootView);
                });
    }

    @Test
    @MediumTest
    public void testColorPicker_forceSingleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setColorPickerLayoutType(ColorPickerLayoutType.SINGLE_ROW);
                    mRootView.addView(mContainerView);
                });

        // Change the width of the parent view to restrict for a double row
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(
                            mContainerView, "TabGroupColorPicker.TestForceSingleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });

        // Validate that a row split was not performed
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);

                    Assert.assertEquals(mColorList.size(), firstRow.getChildCount());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setColorPickerLayoutType(ColorPickerLayoutType.DYNAMIC);
                });
    }

    @Test
    @MediumTest
    public void testColorPicker_forceDoubleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setColorPickerLayoutType(ColorPickerLayoutType.DOUBLE_ROW);
                    mRootView.addView(mContainerView);
                });

        // Change the width of the parent view to allow for a single row
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(
                            mContainerView, "TabGroupColorPicker.TestForceDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });

        // Validate that a row split was performed
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    LinearLayout secondRow =
                            mContainerView.findViewById(R.id.color_picker_second_row);

                    Assert.assertEquals((mColorList.size() + 1) / 2, firstRow.getChildCount());
                    Assert.assertEquals(mColorList.size() / 2, secondRow.getChildCount());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setColorPickerLayoutType(ColorPickerLayoutType.DYNAMIC);
                });
    }

    @Test
    @MediumTest
    public void testColorPicker_dynamicSingleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int selectedIndex = 1;
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    Assert.assertEquals(mColorList.size(), firstRow.getChildCount());

                    for (int color : mColorList) {
                        FrameLayout colorView = (FrameLayout) firstRow.getChildAt(color);
                        ImageView imageView = colorView.findViewById(R.id.color_picker_icon);
                        LayerDrawable layerDrawable = (LayerDrawable) imageView.getBackground();

                        // Check that the default item's selection layer indicates a selection.
                        if (color == mColorList.get(selectedIndex)) {
                            Assert.assertEquals(
                                    0xFF, layerDrawable.getDrawable(SELECTION_LAYER).getAlpha());
                        } else {
                            Assert.assertEquals(
                                    0, layerDrawable.getDrawable(SELECTION_LAYER).getAlpha());
                        }
                    }
                });
    }

    @Test
    @MediumTest
    public void testColorPicker_dynamicAlternateSelection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int selectedIndex = 0;
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);

                    // Mock a click on a new color item.
                    mCoordinator.setSelectedColorItem(mColorList.get(selectedIndex));

                    for (int color : mColorList) {
                        FrameLayout colorView = (FrameLayout) firstRow.getChildAt(color);
                        ImageView imageView = colorView.findViewById(R.id.color_picker_icon);
                        LayerDrawable layerDrawable = (LayerDrawable) imageView.getBackground();

                        // Check that the default item's selection layer indicates a selection.
                        if (color == mColorList.get(selectedIndex)) {
                            Assert.assertEquals(
                                    0xFF, layerDrawable.getDrawable(SELECTION_LAYER).getAlpha());
                        } else {
                            Assert.assertEquals(
                                    0, layerDrawable.getDrawable(SELECTION_LAYER).getAlpha());
                        }
                    }
                });
    }

    @Test
    @MediumTest
    public void testColorPicker_dynamicDoubleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        // Change the width of the parent view to enact a row split on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(mContainerView, "TabGroupColorPicker.TestDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });

        // Validate that a row split was performed
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    LinearLayout secondRow =
                            mContainerView.findViewById(R.id.color_picker_second_row);

                    Assert.assertEquals((mColorList.size() + 1) / 2, firstRow.getChildCount());
                    Assert.assertEquals(mColorList.size() / 2, secondRow.getChildCount());
                });

        // Change the width of the parent view to enact a single row on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(mContainerView, "TabGroupColorPicker.TestDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });

        // Validate that a single row was returned to
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);

                    Assert.assertEquals(mColorList.size(), firstRow.getChildCount());
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testColorPicker_singleRowRender() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        mRenderTestRule.render(mRootView, "tab_group_color_picker_single_row");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testColorPicker_doubleRowRender() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        // Change the width of the parent view to enact a row split on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * (mColorList.size() - 1);

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(mContainerView, "TabGroupColorPicker.TestDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });

        mRenderTestRule.render(mRootView, "tab_group_color_picker_double_row");

        // Change the width of the parent view to enact a single row on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout firstRow =
                            mContainerView.findViewById(R.id.color_picker_first_row);
                    int containerWidthPx =
                            firstRow.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(mContainerView, "TabGroupColorPicker.TestDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });
    }
}
