// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiDisableIf;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the UndoBarController.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UndoBarControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private SnackbarManager mSnackbarManager;
    private TabModel mTabModel;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSnackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        mTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Undo() throws Exception {
        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(1, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        Assert.assertEquals("Closed about:blank", getSnackbarText());
        Assert.assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        Assert.assertEquals(0, mTabModel.getCount());

        clickSnackbar();

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(1, mTabModel.getCount());
    }

    @Test
    @SmallTest
    public void testCloseAll_SingleTab_Dismiss() throws Exception {
        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(1, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        Assert.assertEquals("Closed about:blank", getSnackbarText());
        Assert.assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        Assert.assertEquals(0, mTabModel.getCount());

        dismissSnackbars();

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(0, mTabModel.getCount());
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(2, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        Assert.assertEquals("2 tabs closed", getSnackbarText());
        Assert.assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        Assert.assertEquals(0, mTabModel.getCount());

        clickSnackbar();

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(2, mTabModel.getCount());
    }

    @Test
    @SmallTest
    public void testCloseAll_MultipleTabs_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(2, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        Assert.assertEquals("2 tabs closed", getSnackbarText());
        Assert.assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        Assert.assertEquals(0, mTabModel.getCount());

        dismissSnackbars();

        Assert.assertNull(getCurrentSnackbar());
        Assert.assertEquals(0, mTabModel.getCount());
    }

    @Test
    @SmallTest
    // When both START_SURFACE_ANDROID and TAB_GROUPS_CONTINUATION_ANDROID are enabled, changing
    // accessibility status won't recreate ChromeTabbedActivity.
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.START_SURFACE_WITH_ACCESSIBILITY})
    // clang-format off
    @CommandLineFlags.Add({
            "enable-features=" + ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:gts-low-end-support/true" +
                                               "/gts-accessibility-support/true"})
    // clang-format on
    public void
    testUndoSnackbarDisabled_AccessibilityEnabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Assert.assertNull("Snack bar should be null initially", getCurrentSnackbar());
        Assert.assertEquals(2, mTabModel.getCount());

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Assert.assertNull(
                "Undo snack bar should not be showing in accessibility mode", getCurrentSnackbar());
    }

    @Test
    @SmallTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // crbug/1199248
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_WITH_ACCESSIBILITY})
    // clang-format off
    @CommandLineFlags.Add({
            "enable-features=" + ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:gts-low-end-support/true" +
                                               "/gts-accessibility-support/true"})
    // clang-format on
    public void
    testUndoSnackbarEnabled_AccessibilityEnabledWithGroupM5() throws Exception {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));

        Assert.assertNull("Snack bar should be null initially", getCurrentSnackbar());
        Assert.assertEquals("Tab Model should contain 1 tab", 1, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        Assert.assertEquals("Incorrect snackbar text", "Closed about:blank", getSnackbarText());
        Assert.assertTrue("Incorrect SnackbarController type",
                currentSnackbar.getController() instanceof UndoBarController);
        Assert.assertEquals(
                "Tab Model should contain 0 tab after tab closed", 0, mTabModel.getCount());
    }

    private void clickSnackbar() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mSnackbarManager.onClick(mActivityTestRule.getActivity().findViewById(
                                R.id.snackbar_button)));
    }

    private void dismissSnackbars() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mSnackbarManager.dismissSnackbars(
                                mSnackbarManager.getCurrentSnackbarForTesting().getController()));
    }

    private String getSnackbarText() {
        return ((TextView) mActivityTestRule.getActivity().findViewById(R.id.snackbar_message))
                .getText()
                .toString();
    }

    private Snackbar getCurrentSnackbar() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Snackbar>() {
            @Override
            public Snackbar call() {
                return mSnackbarManager.getCurrentSnackbarForTesting();
            }
        });
    }
}
