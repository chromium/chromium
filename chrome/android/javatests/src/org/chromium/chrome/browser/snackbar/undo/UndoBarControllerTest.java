// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar.undo;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
    @RetryOnFailure
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

    private void clickSnackbar() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSnackbarManager.onClick(mActivityTestRule.getActivity().findViewById(
                                R.id.snackbar_button)));
    }

    private void dismissSnackbars() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSnackbarManager.dismissSnackbars(
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
