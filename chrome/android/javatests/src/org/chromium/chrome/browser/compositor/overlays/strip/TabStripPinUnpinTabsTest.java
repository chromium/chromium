// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.PINNED_TAB_WIDTH_DP;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashSet;
import java.util.Set;

/** Instrumentation tests for pinning and unpinning tabs on LFF tab strip */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP,
    ChromeFeatureList.ANDROID_PINNED_TABS
})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class TabStripPinUnpinTabsTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final float PINNED_TAB_WIDTH_WITH_OVERLAP = PINNED_TAB_WIDTH_DP;
    private static final float TAB_OVERLAP_WIDTH = 28f;
    private static final float PINNED_TAB_WIDTH_WITHOUT_OVERLAP =
            PINNED_TAB_WIDTH_WITH_OVERLAP - TAB_OVERLAP_WIDTH;

    private StripLayoutHelper mStripLayoutHelper;
    private TabModel mTabModel;
    private String mPinTabMenuLabel;
    private String mUnpinTabMenuLabel;
    private String mPinMultipleTabsMenuLabel;
    private String mUnpinMultipleTabsMenuLabel;

    @Before
    public void setUp() throws Exception {
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        mTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
        mPinTabMenuLabel =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.pin_tabs_menu_item, 1);
        mUnpinTabMenuLabel =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.unpin_tabs_menu_item, 1);
        mPinMultipleTabsMenuLabel =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.pin_tabs_menu_item, 2);
        mUnpinMultipleTabsMenuLabel =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.unpin_tabs_menu_item, 2);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabModel.clearMultiSelection(/* notifyObservers= */ false));
    }

    @Test
    @SmallTest
    public void testPinAndUnpin_OneByOne() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        int lastPinnedIndex = 0;
        float expectedDrawX = 0f;

        // Pin all tabs one by one via tab context menu.
        while (lastPinnedIndex < tabs.length) {
            // Pinning from the back: verify tab is in the correct state, then open menu and pin.
            showMenu(tabs.length - 1);
            StripLayoutTab tabToPin = tabs[tabs.length - 1];
            assertFalse("Tab should not be pinned.", tabToPin.getIsPinned());
            onView(withText(mPinTabMenuLabel)).check(matches(isDisplayed()));
            onView(withText(mPinTabMenuLabel)).perform(click());

            // Verify the tab is pinned, moved to the front and has correct width.
            verifyTabIsPinned(tabs, tabToPin, expectedDrawX, lastPinnedIndex);
            expectedDrawX += PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
            lastPinnedIndex++;
        }

        while (lastPinnedIndex > 0) {
            // Unpinning from the start: verify tab is in the correct state, then open menu and
            // unpin.
            lastPinnedIndex--;
            expectedDrawX -= PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
            showMenu(/* tabIndex= */ 0);
            StripLayoutTab tabToUnpin = tabs[0];
            assertTrue("Tab should be pinned.", tabToUnpin.getIsPinned());
            onView(withText(mUnpinTabMenuLabel)).check(matches(isDisplayed()));
            onView(withText(mUnpinTabMenuLabel)).perform(click());

            // Verify the tab is unpinned, moved to the back and has correct width.
            verifyTabIsUnpinned(tabs, tabToUnpin, expectedDrawX, lastPinnedIndex);
        }
    }

    @Test
    @SmallTest
    public void testCloseAndRestorePinnedTab() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        int lastPinnedIndex = 0;
        float expectedDrawX = 0f;

        // Pin all tabs one by one via tab context menu.
        while (lastPinnedIndex < tabs.length) {
            // Pinning from the back: verify tab is in the correct state, then open menu and pin.
            showMenu(tabs.length - 1);
            StripLayoutTab tabToPin = tabs[tabs.length - 1];
            assertFalse("Tab should not be pinned.", tabToPin.getIsPinned());
            onView(withText(mPinTabMenuLabel)).check(matches(isDisplayed()));
            onView(withText(mPinTabMenuLabel)).perform(click());

            // Verify the tab is pinned, moved to the front and has correct width.
            verifyTabIsPinned(tabs, tabToPin, expectedDrawX, lastPinnedIndex);
            expectedDrawX += PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
            lastPinnedIndex++;
        }

        String closeLabel =
                mActivityTestRule.getActivity().getResources().getString(R.string.close);
        String undoLabel = mActivityTestRule.getActivity().getResources().getString(R.string.undo);
        for (int i = tabs.length - 1; i >= 0; i--) {
            showMenu(i);

            // Verify the "Close tab" option is showing in tab context menu, then close the tab.
            onView(withText(closeLabel)).check(matches(isDisplayed()));
            onView(withText(closeLabel)).perform(click());

            // Verify the tab is closed.
            assertEquals(
                    "There are now four tabs present",
                    4,
                    getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel()));

            // Verify the "Undo" option is showing, then undo the closed tab.
            onView(withText(undoLabel)).check(matches(isDisplayed()));
            onView(withText(undoLabel)).perform(click());

            // Verify the tab restored by undo is pinned.
            assertEquals(
                    "There are now five tabs present",
                    5,
                    getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel()));
            assertEquals("Tab should be pinned.", true, tabs[i].getIsPinned());
        }
    }

    @Test
    @SmallTest
    public void testPinGroupedTab() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);

        // Group the last two tabs.
        int firstGroupedIndex = 3;
        int secondGroupedIndex = 4;
        TabStripTestUtils.createTabGroup(
                mActivityTestRule.getActivity(),
                /* isIncognito= */ false,
                firstGroupedIndex,
                secondGroupedIndex);

        // Verify group is created with the last two tabs.
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab tabToPin = tabs[firstGroupedIndex];
        TabGroupModelFilter groupModelFilter =
                TabStripTestUtils.getTabGroupModelFilter(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var tab = mTabModel.getTabById(tabToPin.getTabId());
                    assertEquals(firstGroupedIndex, groupModelFilter.getTabModel().indexOf(tab));
                    assertFalse(tabToPin.getIsPinned());
                    assertNotNull(tab.getTabGroupId());
                });

        // Open menu and pin tab.
        showMenu(firstGroupedIndex);
        assertFalse("Tab should not be pinned.", tabToPin.getIsPinned());
        onView(withText(mPinTabMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mPinTabMenuLabel)).perform(click());

        // Verify the tab being pinned is ungrouped.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Tab should be ungrouped.",
                            groupModelFilter.isTabInTabGroup(
                                    mTabModel.getTabAt(firstGroupedIndex)));
                    var tab = mTabModel.getTabById(tabToPin.getTabId());
                    assertEquals(0, groupModelFilter.getTabModel().indexOf(tab));
                    assertTrue(tabToPin.getIsPinned());
                });

        // Verify the tab is pinned, moved to the front and has correct width.
        verifyTabIsPinned(
                mStripLayoutHelper.getStripLayoutTabsForTesting(),
                tabToPin,
                /* expectedDrawX= */ 0f,
                /* expectedIndex= */ 0);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testPinAndUnpin_AllTabs() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);

        // Multi-select all tabs.
        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        final Set<Integer> tabIds = new HashSet();
        for (int i = 0; i < tabs.length; i++) {
            tabIds.add(tabs[i].getTabId());
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabModel.setTabsMultiSelected(tabIds, /* isSelected= */ true));

        // Show menu to bulk pin tabs.
        showMenu(/* tabIndex= */ 0);
        onView(withText(mPinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mPinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are pinned and has correct position and width.
        float expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            verifyTabIsPinned(tabs, tabs[i], expectedDrawX, i);
            expectedDrawX += PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
        }

        // Show menu to bulk unpin tabs.
        showMenu(/* tabIndex= */ 0);
        onView(withText(mUnpinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mUnpinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are unpinned and has correct position and width.
        expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            verifyTabIsUnpinned(tabs, tabs[i], expectedDrawX, i);
            expectedDrawX += tabs[i].getWidth() - TAB_OVERLAP_WIDTH;
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testPinAndUnpin_MultipleTabs() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);

        // Multi-select last two tabs.
        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        final Set<Integer> tabIds = new HashSet();
        StripLayoutTab firstTabToPin = tabs[tabs.length - 2];
        StripLayoutTab secondTabToPin = tabs[tabs.length - 1];
        tabIds.add(firstTabToPin.getTabId());
        tabIds.add(secondTabToPin.getTabId());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabModel.setTabsMultiSelected(tabIds, /* isSelected= */ true));

        // Show menu to bulk pin tabs.
        showMenu(tabs.length - 1);
        onView(withText(mPinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mPinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are pinned and has correct position and width.
        float expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            if (i < 2) {
                verifyTabIsPinned(tabs, tabs[i], expectedDrawX, i);
                expectedDrawX += PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
            } else {
                verifyTabIsUnpinned(tabs, tabs[i], expectedDrawX, i);
                expectedDrawX += tabs[i].getWidth() - TAB_OVERLAP_WIDTH;
            }
        }

        // Show menu to bulk unpin tabs.
        showMenu(/* tabIndex= */ 0);
        onView(withText(mUnpinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mUnpinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are unpinned and has correct position and width.
        expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            verifyTabIsUnpinned(tabs, tabs[i], expectedDrawX, i);
            expectedDrawX += tabs[i].getWidth() - TAB_OVERLAP_WIDTH;
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testPinAndUnpin_MultipleTabs_MixedPinnedUnPinned_PinTabs() {
        TabStripTestUtils.createTabs(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, /* numOfTabs= */ 5);
        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Pin first tab.
        showMenu(/* tabIndex= */ 0);
        onView(withText(mPinTabMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mPinTabMenuLabel)).perform(click());
        verifyTabIsPinned(tabs, tabs[0], /* expectedDrawX= */ 0, /* expectedIndex= */ 0);

        // Multi-select the first pinned tab and last two unpinned tabs.
        final Set<Integer> tabIds = new HashSet();
        StripLayoutTab tabPinned = tabs[0];
        StripLayoutTab firstTabToPin = tabs[tabs.length - 2];
        StripLayoutTab secondTabToPin = tabs[tabs.length - 1];
        tabIds.add(tabPinned.getTabId());
        tabIds.add(firstTabToPin.getTabId());
        tabIds.add(secondTabToPin.getTabId());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabModel.setTabsMultiSelected(tabIds, /* isSelected= */ true));

        // Verify "Pin tabs" option is shown in mixed state and bulk pin tabs.
        showMenu(tabs.length - 1);
        onView(withText(mPinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mPinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are pinned and has correct position and width.
        float expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            if (i < 3) {
                verifyTabIsPinned(tabs, tabs[i], expectedDrawX, i);
                expectedDrawX += PINNED_TAB_WIDTH_WITHOUT_OVERLAP;
            } else {
                verifyTabIsUnpinned(tabs, tabs[i], expectedDrawX, i);
                expectedDrawX += tabs[i].getWidth() - TAB_OVERLAP_WIDTH;
            }
        }

        // Show menu to bulk unpin tabs.
        showMenu(/* tabIndex= */ 0);
        onView(withText(mUnpinMultipleTabsMenuLabel)).check(matches(isDisplayed()));
        onView(withText(mUnpinMultipleTabsMenuLabel)).perform(click());

        // Verify the multi-selected tabs are unpinned and has correct position and width.
        expectedDrawX = 0f;
        for (int i = 0; i < tabs.length; i++) {
            verifyTabIsUnpinned(tabs, tabs[i], expectedDrawX, i);
            expectedDrawX += tabs[i].getWidth() - TAB_OVERLAP_WIDTH;
        }
    }

    private void showMenu(int tabIndex) {
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab tab = tabs[tabIndex];
        float x = tab.getDrawX() + tab.getWidth() / 2;
        float y = tab.getDrawY() + tab.getHeight() / 2;

        final StripLayoutHelperManager manager =
                mActivityTestRule.getActivity().getLayoutManager().getStripLayoutHelperManager();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                manager.simulateLongPress(x, y);
                            }
                        });
        onViewWaiting(allOf(withId(R.id.tab_group_action_menu_list), isDisplayed()));
    }

    private void verifyTabIsPinned(
            StripLayoutTab[] tabs,
            StripLayoutTab tabToPin,
            float expectedDrawX,
            int expectedIndex) {
        assertTrue("Tab should be pinned.", tabToPin.getIsPinned());
        assertEquals("Pinned tab should appear at front.", tabToPin, tabs[expectedIndex]);
        assertEquals(
                "Pinned tab width is incorrect.",
                PINNED_TAB_WIDTH_WITH_OVERLAP,
                tabToPin.getWidth(),
                0.1f);
        assertEquals("Pinned tab drawX is incorrect.", expectedDrawX, tabToPin.getDrawX(), 0.1f);
    }

    private void verifyTabIsUnpinned(
            StripLayoutTab[] tabs,
            StripLayoutTab tabToUnpin,
            float expectedDrawX,
            int expectedIndex) {
        assertFalse("Tab should not be pinned.", tabToUnpin.getIsPinned());
        assertEquals("Unpinned tab should appear at the back.", tabToUnpin, tabs[expectedIndex]);
        assertTrue(
                "Unpinned tab width is incorrect.",
                PINNED_TAB_WIDTH_WITH_OVERLAP < tabToUnpin.getWidth());
        assertEquals(
                "Unpinned tab drawX is incorrect.", expectedDrawX, tabToUnpin.getDrawX(), 0.1f);
    }
}
