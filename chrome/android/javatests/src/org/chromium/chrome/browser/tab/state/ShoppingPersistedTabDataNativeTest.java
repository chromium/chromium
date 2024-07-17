// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.util.Arrays;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeoutException;

/** Test relating to {@link ShoppingPersistedTabData} where native is not mocked. */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
public class ShoppingPersistedTabDataNativeTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @SmallTest
    @Test
    public void testMaintenance() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.onDeferredStartup();
                });
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        final Tab tab0 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, profile);
        final Tab tab1 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(1, profile);
        final Tab tab2 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(2, profile);
        ShoppingPersistedTabData shoppingPersistedTabData0 =
                ShoppingPersistedTabDataTestUtils.createSavedShoppingPersistedTabDataOnUiThread(
                        tab0);
        ShoppingPersistedTabData shoppingPersistedTabData1 =
                ShoppingPersistedTabDataTestUtils.createSavedShoppingPersistedTabDataOnUiThread(
                        tab1);
        ShoppingPersistedTabData shoppingPersistedTabData2 =
                ShoppingPersistedTabDataTestUtils.createSavedShoppingPersistedTabDataOnUiThread(
                        tab2);
        // Treat Tabs 0 and 2 as being live, assume Tab 1 was destroyed but its stored
        // ShoppingPersistedTabData was not removed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> PersistedTabData.performStorageMaintenance(Arrays.asList(0, 2)));
        verifySPTD(tab0, true);
        verifySPTD(tab1, false);
        verifySPTD(tab2, true);
    }

    private static void verifySPTD(final Tab tab, final boolean expectedExists) {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.from(
                            tab,
                            (res) -> {
                                if (expectedExists) {
                                    Assert.assertNotNull(res);
                                } else {
                                    Assert.assertNull(res.getPriceDrop());
                                }
                                semaphore.release();
                            });
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }
}
