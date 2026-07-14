// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import com.google.android.material.button.MaterialButton;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerContainer;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerCoordinator.TabGroupColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerType;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/** Integration and render tests for the TabGroupColorPicker feature. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabGroupColorPickerTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(3)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private TabGroupColorPickerCoordinator mCoordinator;
    private TabGroupColorPickerContainer mContainerView;
    private FrameLayout mRootView;
    private List<Integer> mColorList;

    public TabGroupColorPickerTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
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

                    View root =
                            LayoutInflater.from(mActivityTestRule.getActivity())
                                    .inflate(
                                            R.layout.tab_group_color_picker_container,
                                            /* root= */ null);
                    TabGroupColorPickerContainer container =
                            root.findViewById(R.id.color_picker_container);
                    mCoordinator =
                            new TabGroupColorPickerCoordinator(
                                    mActivityTestRule.getActivity(),
                                    colors,
                                    container,
                                    TabGroupColorPickerType.TAB_GROUP,
                                    false,
                                    TabGroupColorPickerLayoutType.DYNAMIC,
                                    null);
                    mCoordinator.setSelectedColorItem(colors.get(selectedIndex));
                    mContainerView = (TabGroupColorPickerContainer) mCoordinator.getContainerView();
                    if (mContainerView.getParent() != null) {
                        ((ViewGroup) mContainerView.getParent()).removeView(mContainerView);
                    }
                    mColorList = colors;

                    mRootView = new FrameLayout(activity);
                    activity.setContentView(mRootView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @MediumTest
    public void testTabGroupColorPicker_forceSingleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setTabGroupColorPickerLayoutType(
                            TabGroupColorPickerLayoutType.SINGLE_ROW);
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Change the width of the parent view to restrict for a double row
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

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
                    Assert.assertEquals(mColorList.size(), mContainerView.getChildCount());
                    verifySingleRowLayout();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setTabGroupColorPickerLayoutType(
                            TabGroupColorPickerLayoutType.DYNAMIC);
                });
    }

    @Test
    @MediumTest
    public void testTabGroupColorPicker_forceDoubleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setTabGroupColorPickerLayoutType(
                            TabGroupColorPickerLayoutType.DOUBLE_ROW);
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Change the width of the parent view to allow for a single row
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

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
                    Assert.assertEquals(mColorList.size(), mContainerView.getChildCount());
                    verifyDoubleRowLayout();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setTabGroupColorPickerLayoutType(
                            TabGroupColorPickerLayoutType.DYNAMIC);
                });
    }

    @Test
    @MediumTest
    public void testTabGroupColorPicker_dynamicSingleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(mColorList.size(), mContainerView.getChildCount());

                    int selectedIndex = 1;
                    for (int color : mColorList) {
                        MaterialButton materialButton =
                                (MaterialButton) mContainerView.getChildAt(color);
                        Assert.assertNotNull(materialButton.getBackgroundTintList());
                        Assert.assertNotNull(materialButton.getRippleColor());
                        if (color == selectedIndex) {
                            Assert.assertTrue(materialButton.isChecked());
                        } else {
                            Assert.assertFalse(materialButton.isChecked());
                        }
                    }
                });
    }

    @Test
    @MediumTest
    public void testTabGroupColorPicker_dynamicAlternateSelection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int selectedIndex = 0;

                    // Mock a click on a new color item.
                    mCoordinator.setSelectedColorItem(mColorList.get(selectedIndex));

                    for (int color : mColorList) {
                        MaterialButton button = (MaterialButton) mContainerView.getChildAt(color);
                        if (color == selectedIndex) {
                            Assert.assertTrue(button.isChecked());
                        } else {
                            Assert.assertFalse(button.isChecked());
                        }
                    }
                });
    }

    @Test
    @MediumTest
    public void testTabGroupColorPicker_dynamicDoubleRow() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Change the width of the parent view to enact a row split on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

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
                    Assert.assertEquals(mColorList.size(), mContainerView.getChildCount());
                    verifyDoubleRowLayout();
                });

        // Change the width of the parent view to enact a single row on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

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
                    Assert.assertEquals(mColorList.size(), mContainerView.getChildCount());
                    verifySingleRowLayout();
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTabGroupColorPicker_singleRowRender() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerView.setTabGroupColorPickerLayoutType(
                            TabGroupColorPickerLayoutType.SINGLE_ROW);
                    mRootView.addView(mContainerView);

                    // Set parent width to fit single row to prevent cutting off
                    int width = mContainerView.getSingleRowWidth();
                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = width;
                    mRootView.setLayoutParams(params);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mRenderTestRule.render(mRootView, "tab_group_color_picker_single_row");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTabGroupColorPicker_doubleRowRender() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView.addView(mContainerView);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Change the width of the parent view to enact a row split on the colors
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth()
                                    * (mColorList.size() - 1);

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
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    ViewGroup.LayoutParams params = mRootView.getLayoutParams();
                    params.width = containerWidthPx;
                    mRootView.setLayoutParams(params);
                });

        // Enforce that the change was made to the parent view of the container
        CriteriaHelper.pollUiThread(
                () -> {
                    int containerWidthPx =
                            mContainerView.getChildAt(0).getMeasuredWidth() * mColorList.size();

                    // Refresh the layout and re-measure the widths
                    ViewUtils.requestLayout(mContainerView, "TabGroupColorPicker.TestDoubleRow");
                    Criteria.checkThat(
                            "Width was not set properly",
                            mRootView.getMeasuredWidth(),
                            Matchers.is(containerWidthPx));
                });
    }

    private void verifyDoubleRowLayout() {
        int midPoint = (mColorList.size() + 1) / 2;
        float firstRowY = mContainerView.getChildAt(0).getY();
        float secondRowY = mContainerView.getChildAt(midPoint).getY();
        Assert.assertTrue("Second row should be below first row", secondRowY > firstRowY);

        for (int i = 0; i < midPoint; i++) {
            Assert.assertEquals(
                    "First row item " + i + " has wrong Y",
                    firstRowY,
                    mContainerView.getChildAt(i).getY(),
                    0.1);
        }
        for (int i = midPoint; i < mColorList.size(); i++) {
            Assert.assertEquals(
                    "Second row item " + i + " has wrong Y",
                    secondRowY,
                    mContainerView.getChildAt(i).getY(),
                    0.1);
        }
    }

    private void verifySingleRowLayout() {
        float firstItemY = mContainerView.getChildAt(0).getY();
        for (int i = 1; i < mColorList.size(); i++) {
            Assert.assertEquals(
                    "Item " + i + " has wrong Y (not single row)",
                    firstItemY,
                    mContainerView.getChildAt(i).getY(),
                    0.1);
        }
    }
}
