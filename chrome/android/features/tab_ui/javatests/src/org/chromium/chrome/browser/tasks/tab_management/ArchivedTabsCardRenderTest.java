// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/** Render tests for archived tabs message card. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ArchivedTabsCardRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(4)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    private FrameLayout mContentView;
    private View mArchivedTabsCardView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public ArchivedTabsCardRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    mArchivedTabsCardView =
                            LayoutInflater.from(mActivityTestRule.getActivity())
                                    .inflate(
                                            R.layout.archived_tabs_message_card_view, mContentView);

                    mModel =
                            new PropertyModel.Builder(ALL_KEYS)
                                    .with(NUMBER_OF_ARCHIVED_TABS, 12)
                                    .with(ARCHIVE_TIME_DELTA_DAYS, 14)
                                    .with(
                                            CLICK_HANDLER,
                                            () -> {
                                                mCallbackHelper.notifyCalled();
                                            })
                                    .build();

                    mPropertyModelChangeProcessor =
                            PropertyModelChangeProcessor.create(
                                    mModel,
                                    mArchivedTabsCardView,
                                    ArchivedTabsCardViewBinder::bind);
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlural() throws IOException, InterruptedException {
        mRenderTestRule.render(mArchivedTabsCardView, "plural");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlural_VeryLargeNumbers() throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(NUMBER_OF_ARCHIVED_TABS, 99999999);
                    mModel.set(ARCHIVE_TIME_DELTA_DAYS, 99999999);
                });
        mRenderTestRule.render(mArchivedTabsCardView, "plural_huge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSingular() throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(NUMBER_OF_ARCHIVED_TABS, 1);
                    mModel.set(ARCHIVE_TIME_DELTA_DAYS, 1);
                });

        mRenderTestRule.render(mArchivedTabsCardView, "singular");
    }
}
