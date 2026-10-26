// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.os.Build;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Tests for the UndoBarController. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
public class UndoBarControllerTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mPage;
    private SnackbarManager mSnackbarManager;
    private TabModel mTabModel;

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mSnackbarManager = mPage.getActivity().getSnackbarManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager.dismissAllSnackbars();
                });

        mTabModel = mPage.getTabModel();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityStateTestHelper.uninitializeForTesting();
                });
    }

    @Test
    @SmallTest
    public void testCloseSingleTab_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(mTabModel.getTabAt(1)).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseSingleTab_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_UNDO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(mTabModel.getTabAt(1)).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Undo() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_ALL_UNDO);

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed about:blank");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Dismiss() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_ALL_UNDO);

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed about:blank");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_ALL_UNDO);

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_ALL_UNDO);

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/477995172, crbug.com/571360557): Re-enable.
    public void testCloseTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                    mTabModel.setTabGroupTitle(mTabModel.getTabAt(0).getTabGroupId(), "My group");
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO);

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
                    mTabModel.setTabGroupTitle(mTabModel.getTabAt(0).getTabGroupId(), null);
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testCloseTabGroup_EmptyTitle_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                    mTabModel.setTabGroupTitle(mTabModel.getTabAt(0).getTabGroupId(), "");
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO);

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
                    mTabModel.setTabGroupTitle(mTabModel.getTabAt(0).getTabGroupId(), null);
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testDeleteTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(mTabModel.getTabAt(0), mTabModel.getTabAt(1)),
                            mTabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_GROUP_DELETE_UNDO);

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

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testDeleteTabGroup_WithOtherTab_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_GROUP_DELETE_UNDO);

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

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testCloseTabGroup_WithOtherTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(1));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO);

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

        histogramWatcher.assertExpected();
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
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(
                                    mTabModel.getTabAt(0),
                                    mTabModel.getTabAt(1),
                                    mTabModel.getTabAt(2)),
                            mTabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO);

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

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testDeleteTabGroups_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(1));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(2, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_GROUP_DELETE_UNDO);

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

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testDeleteSingleTabGroup_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(mTabModel.getTabAt(0)).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("1 tab tab group deleted");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(0, getTabGroupCount());

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testDeleteSingleTabGroup_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(mTabModel.getTabAt(0)).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("1 tab tab group deleted");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(0, getTabGroupCount());

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(0, getTabGroupCount());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
    public void testCloseTabGroups_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(0));
                    mTabModel.createSingleTabGroup(mTabModel.getTabAt(1));
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
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testThrottleUndo_UndoOneTabOfHidingGroup_ShowsTabText() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        // Tab 0 stays ungrouped (and open) so the window never reaches 0 tabs. Closing every tab
        // can finish the activity, which commits all pending closures and empties the throttled
        // undo queue before stopThrottling() can show the snackbar.
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));
        Tab tab3 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(3));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2, tab3),
                            tab1,
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                });

        assertNull(getCurrentSnackbar());
        assertEquals(4, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(
                            TabClosureParams.closeTabs(List.of(tab1, tab2, tab3))
                                    .hideTabGroups(true)
                                    .build());
                });

        assertNull(getCurrentSnackbar());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO);

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(4, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertEquals(1, getTabGroupCount());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
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
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
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
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
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
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
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
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
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
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
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
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.R)
    // TODO(crbug.com/571360557): Re-enable.
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
                    mTabModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
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

    // The external resolution tests below keep tab 0 open so the window never reaches 0 tabs.
    // Closing every tab can finish the activity, which commits all pending closures.

    @Test
    @SmallTest
    public void testExternalUndo_AllTabs_DismissesBar() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectIntRecord(
                                "Tab.CloseUndoBar.UndoneExternally", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab1));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExternalCommit_AllTabs_DismissesBar() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        // All of the bar's tabs end committed, so the bar records a dismissal.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectNoRecords("Tab.CloseUndoBar.UndoneExternally")
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertTrue(isClosurePending(tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.commitTabClosure(tab1.getId());
                });

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab1));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCommitAll_DismissesBar_RecordsDismissed() throws Exception {
        // Guard: also passes on main; ensures Dismissed is recorded exactly once.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectNoRecords("Tab.CloseUndoBar.UndoneExternally")
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertTrue(isClosurePending(tab1));

        ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.commitAllTabClosures());

        assertNull(getCurrentSnackbar());
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab1));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExternalUndo_PartialThenRest() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectIntRecord(
                                "Tab.CloseUndoBar.UndoneExternally",
                                Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTabs(List.of(tab1, tab2)).build());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        currentSnackbar = getCurrentSnackbar();
        assertNotNull(currentSnackbar);
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertTrue(isClosurePending(tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab2.getId());
                });

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExternalUndo_PartialThenCommitAll_NoDismissed() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        // A bar partly undone outside of it records an external undo, not a dismissal, when the
        // rest is committed.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectIntRecord(
                                "Tab.CloseUndoBar.UndoneExternally",
                                Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTabs(List.of(tab1, tab2)).build());
                });

        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertNotNull(currentSnackbar);
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertTrue(isClosurePending(tab2));

        ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.commitAllTabClosures());

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab2));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExternalUndo_OneOfTwoBars() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectIntRecord(
                                "Tab.CloseUndoBar.UndoneExternally", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });
        Snackbar snackbarA = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertTrue(snackbarA.getController() instanceof UndoBarController);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab2).build());
                });
        Snackbar snackbarB = getCurrentSnackbar();
        assertNotSame(snackbarA, snackbarB);
        assertTrue(snackbarB.getController() instanceof UndoBarController);
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab2.getId());
                });

        assertSame(snackbarA, getCurrentSnackbar());
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertTrue(isClosurePending(tab1));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab1));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExternalCommitAndUndo_Mixed_DismissesBar() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectNoRecords("Tab.CloseUndoBar.Undone")
                        .expectIntRecord(
                                "Tab.CloseUndoBar.UndoneExternally",
                                Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTabs(List.of(tab1, tab2)).build());
                });

        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.commitTabClosure(tab1.getId());
                });

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertNotNull(currentSnackbar);
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab2.getId());
                });

        assertNull(getCurrentSnackbar());
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));
        assertFalse(isClosurePending(tab1));
        assertFalse(isClosurePending(tab2));

        histogramWatcher.assertExpected();
    }

    // The stale bar tests below fail if a bar's action iterates its original tabs: undoing or
    // committing the stale bar would also resolve the new pending closure of a tab that was undone
    // outside of the bar and then closed again.

    @Test
    @SmallTest
    public void testStaleBar_ReclosedTab_UndoLeavesNewClosure() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        // One record per bar; the UMA type of each bar is fixed when it is shown.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Tab.CloseUndoBar.Undone",
                                Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO,
                                Snackbar.UMA_TAB_CLOSE_UNDO)
                        .expectNoRecords("Tab.CloseUndoBar.Dismissed")
                        .expectNoRecords("Tab.CloseUndoBar.UndoneExternally")
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTabs(List.of(tab1, tab2)).build());
                });

        Snackbar snackbarX = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(snackbarX.getController() instanceof UndoBarController);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        assertSame(snackbarX, getCurrentSnackbar());
        assertFalse(isClosurePending(tab1));
        assertTrue(isClosurePending(tab2));

        // Throttle so that the new bar for tab 1 is queued and the stale bar stays current.
        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        assertSame(snackbarX, getCurrentSnackbar());
        assertTrue(isClosurePending(tab1));
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertFalse(isClosurePending(tab2));
        assertTrue(isClosurePending(tab1));
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        Snackbar snackbarY = getCurrentSnackbar();
        assertNotNull(snackbarY);
        assertNotSame(snackbarX, snackbarY);
        assertTrue(snackbarY.getController() instanceof UndoBarController);
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertFalse(isClosurePending(tab1));
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testStaleBar_ReclosedTab_DismissLeavesNewClosure() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        UndoBarController undoBarController = cta.getUndoBarControllerForTesting();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(1));
        Tab tab2 = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabAt(2));

        assertNull(getCurrentSnackbar());
        assertEquals(3, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        // The UMA type of each bar is fixed when it is shown.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                        .expectIntRecord("Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO)
                        .expectNoRecords("Tab.CloseUndoBar.UndoneExternally")
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTabs(List.of(tab1, tab2)).build());
                });

        Snackbar snackbarX = getCurrentSnackbar();
        assertSnackbarTextEqualsAllowingTruncation("2 tabs closed");
        assertTrue(snackbarX.getController() instanceof UndoBarController);
        Object actionDataX = snackbarX.getActionData();
        assertNotNull(actionDataX);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.cancelTabClosure(tab1.getId());
                });

        assertSame(snackbarX, getCurrentSnackbar());
        assertFalse(isClosurePending(tab1));
        assertTrue(isClosurePending(tab2));

        // Throttle so that the new bar for tab 1 is queued and the stale bar stays current.
        int token = ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.startThrottling());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        assertSame(snackbarX, getCurrentSnackbar());
        assertTrue(isClosurePending(tab1));
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        // Dismiss only the stale bar.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSnackbarManager.dismissSnackbars(undoBarController, actionDataX));

        assertNull(getCurrentSnackbar());
        assertFalse(isClosurePending(tab2));
        assertTrue(isClosurePending(tab1));
        assertEquals(1, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        ThreadUtils.runOnUiThreadBlocking(() -> undoBarController.stopThrottling(token));

        Snackbar snackbarY = getCurrentSnackbar();
        assertNotNull(snackbarY);
        assertNotSame(snackbarX, snackbarY);
        assertTrue(snackbarY.getController() instanceof UndoBarController);
        assertSnackbarTextEqualsAllowingTruncation("Closed New tab");

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertFalse(isClosurePending(tab1));
        assertEquals(2, ChromeTabUtils.getTabCountOnUiThread(mTabModel));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testUndoSnackbarDisabled_AccessibilityEnabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(true));
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
                () -> AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(true));

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
        // Read from the current snackbar view: a previous bar's view can still be attached while
        // its dismiss animation runs, so an activity-wide findViewById may return stale text.
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SnackbarView view = mSnackbarManager.getCurrentSnackbarViewForTesting();
                    assertNotNull("No snackbar view is showing", view);
                    TextView message =
                            view.getContainerViewForTesting().findViewById(R.id.snackbar_message);
                    return message.getText().toString();
                });
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

    private boolean isClosurePending(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.isClosurePending(tab.getId()));
    }

    private int getTabGroupCount() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getTabGroupCount());
    }

    private void closeTabs(TabClosureParams params) {
        mTabModel.getTabRemover().closeTabs(params, /* allowDialog= */ false);
    }
}
