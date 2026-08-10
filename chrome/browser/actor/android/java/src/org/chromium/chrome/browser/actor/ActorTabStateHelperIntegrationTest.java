// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.util.Arrays;
import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests tab model state which changes during activity lifecycle.")
public class ActorTabStateHelperIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(mTab, (String) null);
    }

    @After
    public void tearDown() {
        ActorKeyedServiceFactory.setForTesting(null);
    }

    @Test
    @MediumTest
    public void testCreateAndInsertPlaceholder_Pinned() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

                    tabModel.pinTab(mTab.getId(), /* showUngroupDialog= */ false);
                    assertTrue(mTab.getIsPinned());

                    Tab placeholder =
                            ActorTabStateHelper.createAndInsertPlaceholder(mTab, tabModel);
                    assertNotNull(placeholder);

                    assertTrue(placeholder.getIsPinned());
                });
    }

    @Test
    @MediumTest
    public void testCreateAndInsertPlaceholder_MultiplePinnedTabs() throws Exception {
        Tab tab1 = mActivityTestRule.loadUrlInNewTab("about:blank");
        Tab tab2 = mActivityTestRule.loadUrlInNewTab("about:blank");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

                    tabModel.pinTab(mTab.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                    tabModel.pinTab(tab2.getId(), /* showUngroupDialog= */ false);

                    assertEquals(0, tabModel.indexOf(mTab));
                    assertEquals(1, tabModel.indexOf(tab1));
                    assertEquals(2, tabModel.indexOf(tab2));

                    Tab placeholder1 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab1, tabModel);
                    assertNotNull(placeholder1);

                    assertTrue(placeholder1.getIsPinned());
                    assertEquals(2, tabModel.indexOf(placeholder1));
                    assertEquals(3, tabModel.indexOf(tab2));

                    Tab placeholder0 =
                            ActorTabStateHelper.createAndInsertPlaceholder(mTab, tabModel);
                    assertNotNull(placeholder0);

                    assertTrue(placeholder0.getIsPinned());
                    assertEquals(1, tabModel.indexOf(placeholder0));
                    assertEquals(2, tabModel.indexOf(tab1));

                    Tab placeholder2 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab2, tabModel);
                    assertNotNull(placeholder2);

                    assertTrue(placeholder2.getIsPinned());
                    assertEquals(5, tabModel.indexOf(placeholder2));
                });
    }

    @Test
    @MediumTest
    public void testCreateAndInsertPlaceholder_MultipleGroupedTabs() throws Exception {
        Tab tab1 = mActivityTestRule.loadUrlInNewTab("about:blank");
        Tab tab2 = mActivityTestRule.loadUrlInNewTab("about:blank");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

                    Token tabGroupId = Token.createRandom();
                    List<Tab> groupTabs = Arrays.asList(mTab, tab1, tab2);
                    tabModel.createTabGroupForTabGroupSync(groupTabs, tabGroupId);

                    Tab placeholder1 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab1, tabModel);
                    assertNotNull(placeholder1);

                    List<Tab> tabsInGroup = tabModel.getRelatedTabList(tab1.getId());
                    assertEquals(4, tabsInGroup.size());
                    assertEquals(placeholder1, tabsInGroup.get(2));
                    assertEquals(tabGroupId, placeholder1.getTabGroupId());

                    Tab placeholder0 =
                            ActorTabStateHelper.createAndInsertPlaceholder(mTab, tabModel);
                    assertNotNull(placeholder0);

                    tabsInGroup = tabModel.getRelatedTabList(mTab.getId());
                    assertEquals(5, tabsInGroup.size());
                    assertEquals(placeholder0, tabsInGroup.get(1));
                    assertEquals(tabGroupId, placeholder0.getTabGroupId());

                    Tab placeholder2 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab2, tabModel);
                    assertNotNull(placeholder2);

                    tabsInGroup = tabModel.getRelatedTabList(tab2.getId());
                    assertEquals(6, tabsInGroup.size());
                    assertEquals(placeholder2, tabsInGroup.get(5));
                    assertEquals(tabGroupId, placeholder2.getTabGroupId());
                });
    }

    @Test
    @MediumTest
    public void testCreateAndInsertPlaceholder_SingleTab() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

                    assertEquals(1, tabModel.getCount());
                    assertEquals(mTab, tabModel.getTabAt(0));

                    Tab placeholder =
                            ActorTabStateHelper.createAndInsertPlaceholder(mTab, tabModel);
                    assertNotNull(placeholder);

                    assertEquals(2, tabModel.getCount());
                    assertEquals(mTab, tabModel.getTabAt(0));
                    assertEquals(placeholder, tabModel.getTabAt(1));
                });
    }

    @Test
    @MediumTest
    public void testCreateAndInsertPlaceholder_MultipleTabsFirstMiddleLast() throws Exception {
        Tab tab1 = mActivityTestRule.loadUrlInNewTab("about:blank");
        Tab tab2 = mActivityTestRule.loadUrlInNewTab("about:blank");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

                    assertEquals(0, tabModel.indexOf(mTab));
                    assertEquals(1, tabModel.indexOf(tab1));
                    assertEquals(2, tabModel.indexOf(tab2));

                    Tab placeholder1 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab1, tabModel);
                    assertNotNull(placeholder1);

                    assertEquals(1, tabModel.indexOf(tab1));
                    assertEquals(2, tabModel.indexOf(placeholder1));
                    assertEquals(3, tabModel.indexOf(tab2));

                    Tab placeholder0 =
                            ActorTabStateHelper.createAndInsertPlaceholder(mTab, tabModel);
                    assertNotNull(placeholder0);

                    assertEquals(0, tabModel.indexOf(mTab));
                    assertEquals(1, tabModel.indexOf(placeholder0));
                    assertEquals(2, tabModel.indexOf(tab1));

                    Tab placeholder2 =
                            ActorTabStateHelper.createAndInsertPlaceholder(tab2, tabModel);
                    assertNotNull(placeholder2);

                    assertEquals(4, tabModel.indexOf(tab2));
                    assertEquals(5, tabModel.indexOf(placeholder2));
                });
    }
}
