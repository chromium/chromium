// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.TITLE_1;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.TITLE_2;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.TITLE_3;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.TITLE_4;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.TITLE_5;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.buildIconRow;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.buildMenuItem;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuAdapterTest.buildTitleMenuItem;

import android.graphics.drawable.Drawable;
import android.support.test.filters.MediumTest;
import android.support.v7.content.res.AppCompatResources;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Render tests for {@link AppMenuAdapter}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class AppMenuAdapterRenderTest extends DummyUiActivityTestCase {
    // TODO(twellington): Add night mode variant after NightModeTestUtils methods are modularized
    //                    in https://crbug.com/1002287.

    /**
     * Parameter set controlling whether the menu item being rendered is enabled.
     */
    public static class EnabledParams implements ParameterProvider {
        private static List<ParameterSet> sEnabledDisabledParams =
                Arrays.asList(new ParameterSet().value(false).name("Enabled"),
                        new ParameterSet().value(true).name("Disabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sEnabledDisabledParams;
        }
    }

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testStandardMenuItem(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        MenuItem item = buildMenuItem(1, TITLE_1, enabled);
        mRenderTestRule.render(createView(item), "standard");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testStandardMenuItem_Icon(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        Drawable icon =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_ic_vintage_filter);
        MenuItem item = buildMenuItem(1, TITLE_1, enabled, icon);
        mRenderTestRule.render(createView(item), "standard_with_icon");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testTitleButtonMenuItem_Icon(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        Drawable icon =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_ic_vintage_filter);
        MenuItem item = buildTitleMenuItem(1, 2, TITLE_2, 3, TITLE_3, icon, false, false, enabled);
        mRenderTestRule.render(createView(item), "title_button_icon");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testTitleButtonMenuItem_Checkbox_Checked(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        MenuItem item = buildTitleMenuItem(1, 2, TITLE_2, 3, TITLE_3, null, true, true, enabled);
        mRenderTestRule.render(createView(item), "title_button_checkbox_checked");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testTitleButtonMenuItem_Checkbox_Unchecked(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        MenuItem item = buildTitleMenuItem(1, 2, TITLE_2, 3, TITLE_3, null, true, false, enabled);
        mRenderTestRule.render(createView(item), "title_button_checkbox_unchecked");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(EnabledParams.class)
    public void testIconRow_ThreeIcons(boolean enabled) throws IOException {
        setRenderTestPrefix(enabled);
        Drawable icon1 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_forward_black_24dp);
        Drawable icon2 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_star_border_black_24dp);
        Drawable icon3 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_downward_black_24dp);
        MenuItem item = buildIconRow(1, 2, TITLE_1, icon1, 3, TITLE_2, icon2, 4, TITLE_3, icon3, 0,
                "", null, 0, "", null, enabled);

        mRenderTestRule.render(createView(item), "iconrow_three_icons");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIconRow_FourIcons() throws IOException {
        setRenderTestPrefix(true);
        Drawable icon1 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_forward_black_24dp);
        Drawable icon2 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_star_border_black_24dp);
        Drawable icon3 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_downward_black_24dp);
        Drawable icon4 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_info_outline_black_24dp);
        MenuItem item = buildIconRow(1, 2, TITLE_1, icon1, 3, TITLE_2, icon2, 4, TITLE_3, icon3, 5,
                TITLE_4, icon4, 0, "", null, true);

        mRenderTestRule.render(createView(item), "iconrow_four_icons");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIconRow_FiveIcons() throws IOException {
        setRenderTestPrefix(true);
        Drawable icon1 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_forward_black_24dp);
        Drawable icon2 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_star_border_black_24dp);
        Drawable icon3 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_arrow_downward_black_24dp);
        Drawable icon4 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_info_outline_black_24dp);
        Drawable icon5 = AppCompatResources.getDrawable(
                getActivity(), R.drawable.test_ic_refresh_black_24dp);
        MenuItem item = buildIconRow(1, 2, TITLE_1, icon1, 3, TITLE_2, icon2, 4, TITLE_3, icon3, 5,
                TITLE_4, icon4, 6, TITLE_5, icon5, true);

        mRenderTestRule.render(createView(item), "iconrow_five_icons");
    }

    private void setRenderTestPrefix(boolean enabled) {
        mRenderTestRule.setVariantPrefix(enabled ? "Enabled" : "Disabled");
    }

    private View createView(MenuItem item) {
        List<MenuItem> items = new ArrayList<>();
        items.add(item);
        AppMenuAdapter adapter = new AppMenuAdapter(new AppMenuAdapterTest.TestClickHandler(),
                items, getActivity().getLayoutInflater(), 0, null);

        // Create a new FrameLayout to set as the main content view.
        FrameLayout parentView = new FrameLayout(getActivity());
        parentView.setLayoutParams(new FrameLayout.LayoutParams(
                getActivity().getResources().getDimensionPixelSize(R.dimen.menu_width),
                FrameLayout.LayoutParams.WRAP_CONTENT));

        // Create a view for the provided menu item and attach it to the content view.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View view = adapter.getView(0, null, parentView);
            parentView.addView(view);
            getActivity().setContentView(parentView);
        });

        return parentView.getChildAt(0);
    }
}
