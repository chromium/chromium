// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class TabGroupSyncServiceFactoryTest {

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @MediumTest
    public void testSettingTestFactory() throws TimeoutException {
        TabGroupSyncService testService = new TestTabGroupSyncService();
        TabGroupSyncServiceFactory.setForTesting(testService);
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        TabGroupSyncService tabGroupSyncService =
                                TabGroupSyncServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertNotNull(tabGroupSyncService);
                        Assert.assertEquals(tabGroupSyncService, testService);
                    }
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
    public void testServiceCreation_RealService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        TabGroupSyncService tabGroupSyncService =
                                TabGroupSyncServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertNotNull(tabGroupSyncService);
                    }
                });
    }
}
