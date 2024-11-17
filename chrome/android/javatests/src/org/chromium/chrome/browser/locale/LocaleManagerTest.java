// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.policy.test.annotations.Policies;

import java.util.concurrent.ExecutionException;

/** Integration tests for {@link LocaleManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocaleManagerTest {
    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @BeforeClass
    public static void setUpClass() throws ExecutionException {
        // Launch any activity as an Activity ref is required to attempt to show the activity.
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                sActivityTestRule.getActivity().getSnackbarManager()::dismissAllSnackbars);
    }

    @Policies.Add({@Policies.Item(key = "DefaultSearchProviderEnabled", string = "false")})
    @SmallTest
    @Test
    public void testShowSearchEnginePromoDseDisabled() throws Exception {
        final CallbackHelper getShowTypeCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        LocaleManager.getInstance()
                                .setDelegateForTest(
                                        new LocaleManagerDelegate() {
                                            @Override
                                            public int getSearchEnginePromoShowType() {
                                                getShowTypeCallback.notifyCalled();
                                                return SearchEnginePromoType.DONT_SHOW;
                                            }
                                        }));

        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        LocaleManager.getInstance()
                                .showSearchEnginePromoIfNeeded(
                                        sActivityTestRule.getActivity(),
                                        result -> {
                                            Assert.assertTrue(result);
                                            searchEnginesFinalizedCallback.notifyCalled();
                                        }));
        searchEnginesFinalizedCallback.waitForCallback(0);
        Assert.assertEquals(0, getShowTypeCallback.getCallCount());
    }

    @MediumTest
    @Test
    public void testSnackbarForDeviceSearchEngineUpdate() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocaleManager localeManager = LocaleManager.getInstance();
                    SnackbarManager snackbarManager =
                            sActivityTestRule.getActivity().getSnackbarManager();
                    localeManager.setSnackbarManager(snackbarManager);

                    localeManager.showSnackbarForDeviceSearchEngineUpdate();

                    Assert.assertTrue(snackbarManager.isShowing());
                    Snackbar currentSnackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Assert.assertEquals(
                            currentSnackbar.getIdentifierForTesting(),
                            Snackbar.UMA_SEARCH_ENGINE_CHANGED_NOTIFICATION);
                });
    }

    @MediumTest
    @Test
    public void testSnackbarForDeviceSearchEngineUpdateShownAfterSnackbarManagerSet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocaleManager localeManager = LocaleManager.getInstance();
                    SnackbarManager snackbarManager =
                            sActivityTestRule.getActivity().getSnackbarManager();
                    // Simulate the case when snackbar manager is not set.
                    localeManager.setSnackbarManager(null);
                    localeManager.showSnackbarForDeviceSearchEngineUpdate();
                    Assert.assertFalse(snackbarManager.isShowing());

                    localeManager.setSnackbarManager(snackbarManager);

                    Assert.assertTrue(snackbarManager.isShowing());
                    Snackbar currentSnackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Assert.assertEquals(
                            currentSnackbar.getIdentifierForTesting(),
                            Snackbar.UMA_SEARCH_ENGINE_CHANGED_NOTIFICATION);
                });
    }
}
