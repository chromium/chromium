// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIntegrationTestHelper.GroupInfo;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIntegrationTestHelper.TabInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.SavedTabGroup.SavedTabGroupColor;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * On-device sync integration tests for tab group sync from local to remote. These tests use public
 * transit where possible.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(b/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_GROUP_PANE_ANDROID
})
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class TabGroupSyncLocalToRemoteTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mSyncTestRule);

    private TabGroupSyncIntegrationTestHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new TabGroupSyncIntegrationTestHelper(mSyncTestRule);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForHistorySyncEnabled();
        mHelper.assertSyncEntityCount(0);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    @DisabledTest(message = "crbug.com/353952795")
    public void testCreateTabGroup() {
        WebPageStation firstPage = mTransitEntryPoints.alreadyStartedOnBlankPageNonBatched();
        Tab firstTab = firstPage.getLoadedTab();
        int firstTabId = firstTab.getId();
        String firstTabTitle = ChromeTabUtils.getTitleOnUiThread(firstTab);
        String firstTabUrl = ChromeTabUtils.getUrlStringOnUiThread(firstTab);

        RegularNewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        Tab secondTab = secondPage.getLoadedTab();
        int secondTabId = secondTab.getId();
        String secondTabTitle = ChromeTabUtils.getTitleOnUiThread(secondTab);
        String secondTabUrl = ChromeTabUtils.getUrlStringOnUiThread(secondTab);

        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        String title = "test_tab_group_name";
        NewTabGroupDialogFacility dialog =
                editor.openAppMenuWithEditor().groupTabsWithParityEnabled();
        dialog = dialog.inputName(title);
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        // Verify sync.
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        List<SyncEntity> entities = mHelper.getSyncEntities();
        assertEquals("Number of sync entities don't match", 3, entities.size());
        mHelper.assertSyncEntityCount(3);

        GroupInfo group1 = new GroupInfo(title, SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_RED);
        TabInfo tab1 = new TabInfo(firstTabTitle, firstTabUrl, 1);
        TabInfo tab2 = new TabInfo(secondTabTitle, secondTabUrl, 2);
        GroupInfo[] expectedGroups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {group1}, new TabInfo[][] {{tab1, tab2}});
        mHelper.verifySyncEntities(expectedGroups);

        assertFinalDestination(tabSwitcher);
    }
}
