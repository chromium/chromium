// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ListView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicTaskMenuCoordinator.ButtonSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Render tests for {@link GlicTaskMenuCoordinator}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.UNIT_TESTS)
public class GlicTaskMenuRenderTest {
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int CLOSED_TAB_ID = 4;
    private static final int RENDER_REVISION = 2;

    private static final String STANDARD_TASK_TITLE = "Book flight to NYC";
    private static final String REVIEW_TASK_TITLE = "Confirm hotel booking";
    private static final String LONG_TASK_TITLE =
            "A very long task title that exceeds the maximum width of the dropdown menu and"
                    + " should be ellipsized cleanly";

    @ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(RENDER_REVISION)
                    .setDescription("Ligatures and contextual alternates disabled in UI text")
                    .build();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private GlicButtonDelegate mToggleGlicCallback;

    private View mView;

    public GlicTaskMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        Tab tab3 = mock(Tab.class);
        doReturn(tab1).when(mTabModelSelector).getTabById(TAB_ID_1);
        doReturn(tab2).when(mTabModelSelector).getTabById(TAB_ID_2);
        doReturn(tab3).when(mTabModelSelector).getTabById(TAB_ID_3);
        doReturn(null).when(mTabModelSelector).getTabById(CLOSED_TAB_ID);
    }

    @After
    public void tearDown() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ToolbarMenu() throws Exception {
        ActorTask standardTask = mock(ActorTask.class);
        doReturn(STANDARD_TASK_TITLE).when(standardTask).getTitle();
        doReturn(ActorTaskState.ACTING).when(standardTask).getState();
        doReturn(Collections.singleton(TAB_ID_1)).when(standardTask).getLastActedTabs();

        ActorTask reviewTask = mock(ActorTask.class);
        doReturn(REVIEW_TASK_TITLE).when(reviewTask).getTitle();
        doReturn(ActorTaskState.WAITING_ON_USER).when(reviewTask).getState();
        doReturn(Collections.singleton(TAB_ID_2)).when(reviewTask).getLastActedTabs();

        ActorTask longTitleTask = mock(ActorTask.class);
        doReturn(LONG_TASK_TITLE).when(longTitleTask).getTitle();
        doReturn(ActorTaskState.ACTING).when(longTitleTask).getState();
        doReturn(Collections.singleton(TAB_ID_3)).when(longTitleTask).getLastActedTabs();

        renderMenu(
                ButtonSource.TOOLBAR,
                Arrays.asList(standardTask, reviewTask, longTitleTask),
                "glic_task_menu_toolbar",
                /* hoveredIndex= */ -1);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_TabStripMenu() throws Exception {
        ActorTask standardTask = mock(ActorTask.class);
        doReturn(STANDARD_TASK_TITLE).when(standardTask).getTitle();
        doReturn(ActorTaskState.ACTING).when(standardTask).getState();
        doReturn(Collections.singleton(TAB_ID_1)).when(standardTask).getLastActedTabs();

        ActorTask reviewTask = mock(ActorTask.class);
        doReturn(REVIEW_TASK_TITLE).when(reviewTask).getTitle();
        doReturn(ActorTaskState.WAITING_ON_USER).when(reviewTask).getState();
        doReturn(Collections.singleton(TAB_ID_2)).when(reviewTask).getLastActedTabs();

        ActorTask closedTabTask = mock(ActorTask.class);
        doReturn(LONG_TASK_TITLE).when(closedTabTask).getTitle();
        doReturn(ActorTaskState.FINISHED).when(closedTabTask).getState();
        doReturn(true).when(closedTabTask).isCompleted();
        doReturn(Collections.singleton(CLOSED_TAB_ID)).when(closedTabTask).getLastActedTabs();

        renderMenu(
                ButtonSource.TAB_STRIP,
                Arrays.asList(standardTask, reviewTask, closedTabTask),
                "glic_task_menu_tab_strip",
                /* hoveredIndex= */ 0);
    }

    private void renderMenu(
            @ButtonSource int buttonSource,
            List<ActorTask> tasks,
            String renderId,
            int hoveredIndex)
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    GlicTaskMenuCoordinator coordinator =
                            new GlicTaskMenuCoordinator(
                                    activity,
                                    () -> mTabModelSelector,
                                    mToggleGlicCallback,
                                    GlicInvocationSource.TOP_CHROME_BUTTON,
                                    buttonSource);

                    mView = coordinator.createContentView(tasks);
                    if (mView.getParent() != null) {
                        ((ViewGroup) mView.getParent()).removeView(mView);
                    }
                    mView.setBackground(
                            AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
                    activity.setContentView(
                            mView,
                            new LayoutParams(coordinator.getMenuWidthPxForTesting(), WRAP_CONTENT));
                });

        CriteriaHelper.pollUiThread(
                () -> mView.getWidth() > 0 && mView.getHeight() > 0, "View not rendered");

        if (hoveredIndex >= 0) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        ListView listView = mView.findViewById(R.id.menu_list);
                        View hoveredView = listView.getChildAt(hoveredIndex);
                        hoveredView.setHovered(true);
                        hoveredView.jumpDrawablesToCurrentState();
                    });
        }

        mRenderTestRule.render(mView, renderId);
    }
}
