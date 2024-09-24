// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for PriceCardView. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class PriceCardViewTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_SHOPPING_PRICE_TRACKING)
                    .build();

    public PriceCardViewTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    private PriceCardView mPriceCardView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup view = new FrameLayout(getActivity());
                    getActivity().setContentView(view, params);

                    ViewGroup tabView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_grid_card_item, view, false);
                    ((TabGridView) tabView).setTabActionState(TabActionState.CLOSABLE);
                    tabView.setVisibility(View.VISIBLE);
                    view.addView(tabView);

                    mPriceCardView = tabView.findViewById(R.id.price_info_box_outer);
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/1297341")
    public void testRenderPriceCardView() throws IOException {
        mPriceCardView.setPriceStrings("$50", "$100");
        mPriceCardView.setVisibility(View.VISIBLE);

        CriteriaHelper.pollUiThread(() -> mPriceCardView.getVisibility() == View.VISIBLE);

        mRenderTestRule.render(mPriceCardView, "price_card_view");
    }
}
