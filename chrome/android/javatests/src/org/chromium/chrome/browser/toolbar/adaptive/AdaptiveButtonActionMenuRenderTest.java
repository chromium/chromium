// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for adaptive test long-press menu popup. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:mode/always-share"
})
public class AdaptiveButtonActionMenuRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_TOOLBAR)
                    .build();

    private View mView;

    public AdaptiveButtonActionMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUpTest() throws Exception {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    AdaptiveButtonActionMenuCoordinator coordinator =
                            new AdaptiveButtonActionMenuCoordinator();

                    coordinator.displayMenu(
                            activity,
                            new ListMenuButton(activity, null),
                            coordinator.buildMenuItems(),
                            null);

                    mView = coordinator.getContentViewForTesting();
                    if (mView.getParent() != null) {
                        ((ViewGroup) mView.getParent()).removeView(mView);
                    }

                    int popupWidth =
                            activity.getResources()
                                    .getDimensionPixelSize(R.dimen.tab_switcher_menu_width);
                    mView.setBackground(
                            AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
                    activity.setContentView(mView, new LayoutParams(popupWidth, WRAP_CONTENT));
                });
    }

    @After
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> NightModeTestUtils.tearDownNightModeForBlankUiTestActivity());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_TabSwitcherActionMenu() throws IOException {
        mRenderTestRule.render(mView, "adaptive_button_action_menu");
    }
}
