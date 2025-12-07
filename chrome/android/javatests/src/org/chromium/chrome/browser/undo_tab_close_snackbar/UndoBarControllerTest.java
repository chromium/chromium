// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Tests for the UndoBarController. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UndoBarControllerTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mPage;
    private SnackbarManager mSnackbarManager;
    private TabModel mTabModel;
    private TabGroupModelFilter mTabGroupModelFilter;

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mSnackbarManager = mPage.getActivity().getSnackbarManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager.dismissAllSnackbars();
                });

        mTabGroupModelFilter = mPage.getTabGroupModelFilter();
        mTabModel = mPage.getTabModel();
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Undo() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed about:blank");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Dismiss() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed about:blank");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
    }

    @Test
    @SmallTest
    public void testCloseTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    mTabGroupModelFilter.setTabGroupTitle(
                            mTabModel.getTabAt(0).getTabGroupId(), "My group");
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .hideTabGroups(true)
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("My group tab group closed and saved");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.setTabGroupTitle(
                            mTabModel.getTabAt(0).getTabGroupId(), null);
                });
    }

    @Test
    @SmallTest
    public void testCloseTabGroup_EmptyTitle_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    mTabGroupModelFilter.setTabGroupTitle(
                            mTabModel.getTabAt(0).getTabGroupId(), "");
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .hideTabGroups(true)
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs tab group closed and saved");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.setTabGroupTitle(
                            mTabModel.getTabAt(0).getTabGroupId(), null);
                });
    }

    @Test
    @SmallTest
    public void testDeleteTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs tab group deleted");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testDeleteTabGroup_WithOtherTab_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(0));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(
                                                    mTabModel.getTabAt(0),
                                                    mTabModel.getTabAt(1),
                                                    mTabModel.getTabAt(2)))
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("1 tab group, 2 tabs deleted");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testCloseTabGroup_WithOtherTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(1));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(
                                                    mTabModel.getTabAt(0),
                                                    mTabModel.getTabAt(1),
                                                    mTabModel.getTabAt(2)))
                                    .hideTabGroups(true)
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tab groups, 1 tab closed and saved");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testPartialDeleteTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(
                                    mTabModel.getTabAt(0),
                                    mTabModel.getTabAt(1),
                                    mTabModel.getTabAt(2)),
                            mTabModel.getTabAt(0),
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testDeleteTabGroups_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(1));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tab groups deleted");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testCloseTabGroups_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabGroupModelFilter.createSingleTabGroup(mTabModel.getTabAt(1));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(
                                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)))
                                    .hideTabGroups(true)
                                    .build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tab groups closed and saved");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());
    }

    @Test
    @SmallTest
    public void testThrottleUndo() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab0).build());
                    closeTabs(
                            TabClosureParams.closeTabs(List.of(tab1, tab2))
                                    .hideTabGroups(true)
                                    .build());
                });

        assertNull(getCurrentSnackbar());

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        clickSnackbar();

        currentSnackbar = getCurrentSnackbar();
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        clickSnackbar();
    }

    @Test
    @SmallTest
    public void testThrottleUndo_CommitSubset() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab0).build());
                    closeTabs(
                            TabClosureParams.closeTabs(List.of(tab1, tab2))
                                    .hideTabGroups(true)
                                    .build());
                });

        assertNull(getCurrentSnackbar());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.commitTabClosure(tab0.getId());
                    mTabModel.commitTabClosure(tab1.getId());
                });

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        clickSnackbar();

        assertNull(getCurrentSnackbar());
    }

    @Test
    @SmallTest
    public void testThrottleUndo_CommitAll() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab0).build());
                    closeTabs(
                            TabClosureParams.closeTabs(List.of(tab1, tab2))
                                    .hideTabGroups(true)
                                    .build());
                });

        assertNull(getCurrentSnackbar());

        ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.commitAllTabClosures());

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        assertNull(getCurrentSnackbar());
    }

    @Test
    @SmallTest
    public void testThrottleUndo_UndoNotViaSnackbar() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab0).build());
                    closeTabs(
                            TabClosureParams.closeTabs(List.of(tab1, tab2))
                                    .hideTabGroups(true)
                                    .build());
                });

        assertNull(getCurrentSnackbar());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab0.getId());
                    mTabModel.cancelTabClosure(tab1.getId());
                    mTabModel.cancelTabClosure(tab2.getId());
                });

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        assertNull(getCurrentSnackbar());
    }

    @Test
    @SmallTest
    public void testUndoSnackbarDisabled_AccessibilityEnabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull("Snack bar should be null initially", getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(
                "Undo snack bar should not be showing in accessibility mode", getCurrentSnackbar());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testUndoSnackbarEnabled_AccessibilityEnabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));

        assertNull("Snack bar should be null initially", getCurrentSnackbar());
        assertEquals(
                "Tab Model should contain 1 tab",
                1,
                ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed about:blank");
        assertTrue(
                "Incorrect SnackbarController type",
                currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(
                "Tab Model should contain 0 tab after tab closed",
                0,
                ChromeTabUtils.getTabCountOnUiThread(mTabModel));
    }

    private void clickSnackbar() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSnackbarManager.onClick(
                                mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.snackbar_button)));
    }

    private void dismissSnackbars() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSnackbarManager.dismissSnackbars(
                                mSnackbarManager.getCurrentSnackbarForTesting().getController()));
    }

    private void assertSnackbarTextEqualsAllowingTruncation(String expected) {
        assertEquals("Expected text should not contain ellipsis.", -1, expected.indexOf("…"));
        String actual = getSnackbarText();
        int index = actual.indexOf("…");
        if (index != -1) {
            assertEquals(
                    "First part of truncated snackbar text mismatched",
                    expected.substring(0, index),
                    actual.substring(0, index));
            // Skip the ellipsis.
            String actualEnd = actual.substring(index + 1);
            // End of the expected text should be present.
            String expectedEnd = expected.substring(expected.length() - actualEnd.length());
            assertEquals(
                    "Last part of truncated snackbar text mismatched",
                    expectedEnd,
                    actualEnd);
        } else {
            assertEquals("Incorrect snackbar text", expected, actual);
        }
    }

    private String getSnackbarText() {
        return ((TextView) mActivityTestRule.getActivity().findViewById(R.id.snackbar_message))
                .getText()
                .toString();
    }

    private Snackbar getCurrentSnackbar() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public Snackbar call() {
                        return mSnackbarManager.getCurrentSnackbarForTesting();
                    }
                });
    }

    private int getTabGroupCount() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mTabGroupModelFilter.getTabGroupCount());
    }

    private void closeTabs(TabClosureParams params) {
        mTabModel.getTabRemover().closeTabs(params, /* allowDialog= */ false);
    }
}
