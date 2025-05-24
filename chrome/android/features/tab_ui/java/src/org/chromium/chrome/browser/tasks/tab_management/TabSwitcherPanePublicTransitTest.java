// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

/** Public transit tests for the Hub's tab switcher panes. */
// TODO(crbug/324919909): Migrate more tests from TabSwitcherLayoutTest to here or other test
// fixtures before cleaning up those tests. This is partially dependent on public transit supporting
// more core UI elements; for example undo snackbars.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@ImportantFormFactors(DeviceFormFactor.TABLET)
public class TabSwitcherPanePublicTransitTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testSwitchTabModel_ScrollToSelectedTab() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();

        PageStation page = firstPage;
        for (int i = 1; i < 10; i++) {
            page = page.openNewTabFast();
        }
        assertEquals(9, cta.getCurrentTabModel().index());
        IncognitoNewTabPageStation incognitoNtp = page.openNewIncognitoTabFast();
        assertTrue(cta.getCurrentTabModel().isIncognito());

        IncognitoTabSwitcherStation incognitoTabSwitcher = incognitoNtp.openIncognitoTabSwitcher();
        RegularTabSwitcherStation regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);

        LinearLayoutManager layoutManager =
                (LinearLayoutManager)
                        regularTabSwitcher.recyclerViewElement.get().getLayoutManager();
        assertEquals(9, layoutManager.findLastVisibleItemPosition());

        // Go back to a tab to cleanup tab state
        regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
    }

    @Test
    @MediumTest
    public void testTabListEditor_EnterAndExit() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();

        RegularTabSwitcherStation regularTabSwitcher = firstPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility listEditor =
                regularTabSwitcher.openAppMenu().clickSelectTabs();

        listEditor.pressBackToExit();

        // Go back to a tab to cleanup tab state
        regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
    }

    @Test
    @MediumTest
    public void testEmptyStateView() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();

        IncognitoNewTabPageStation incognitoNtp = firstPage.openNewIncognitoTabFast();
        assertTrue(cta.getCurrentTabModel().isIncognito());

        IncognitoTabSwitcherStation incognitoTabSwitcher = incognitoNtp.openIncognitoTabSwitcher();
        onView(RegularTabSwitcherStation.EMPTY_STATE_TEXT).check(doesNotExist());

        RegularTabSwitcherStation regularTabSwitcher = incognitoTabSwitcher.selectRegularTabsPane();

        regularTabSwitcher = regularTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        onView(RegularTabSwitcherStation.EMPTY_STATE_TEXT).check(matches(isDisplayed()));

        incognitoTabSwitcher = regularTabSwitcher.selectIncognitoTabsPane();
        onView(RegularTabSwitcherStation.EMPTY_STATE_TEXT).check(doesNotExist());

        regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        onView(RegularTabSwitcherStation.EMPTY_STATE_TEXT).check(matches(isDisplayed()));

        // Go back to a tab to cleanup tab state
        regularTabSwitcher.openAppMenu().openNewTab();
    }
}
