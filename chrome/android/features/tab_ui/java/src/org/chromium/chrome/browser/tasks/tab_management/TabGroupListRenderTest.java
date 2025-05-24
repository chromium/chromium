// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupPaneStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.test.util.RenderTestRule.Component;

/** Render tests for {@link TabGroupListView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE)
@DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabGroupListRenderTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(1)
                    .build();

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testGroupPane() throws Exception {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();

        createGroupProgrammatic("Group 1", /* wait= */ false);

        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();
        TabGroupPaneStation tabGroupPane = tabSwitcher.selectTabGroupsPane();

        RecyclerView recyclerView = tabGroupPane.recyclerViewElement.get();
        mRenderTestRule.render(recyclerView, "1_group");

        createGroupProgrammatic("Group 2", /* wait= */ true);
        mRenderTestRule.render(recyclerView, "2_groups");

        createGroupProgrammatic("Group 3", /* wait= */ true);
        mRenderTestRule.render(recyclerView, "3_groups");

        tabSwitcher = tabGroupPane.selectRegularTabsPane();

        // Exit to reset.
        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();
        RegularNewTabPageStation newTab = appMenu.openNewTab();
        assertFinalDestination(newTab);
    }

    private void createGroupProgrammatic(String title, boolean wait) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModelSelector selector = cta.getTabModelSelector();
                    TabGroupModelFilter filter =
                            selector.getTabGroupModelFilterProvider().getTabGroupModelFilter(false);
                    TabModel model = cta.getTabModelSelector().getModel(false);
                    Tab tab =
                            model.getTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams("about:blank"),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            null);
                    filter.createSingleTabGroup(tab);
                    filter.setTabGroupTitle(tab.getRootId(), title);
                });
        if (wait) {
            onViewWaiting(withText(title)).check(matches(isDisplayed()));
            // Despite waiting on the view to be visible for some reason only a blank render is
            // captured. Sleep a bit to let drawing finish.
            Thread.sleep(300);
        }
    }
}
