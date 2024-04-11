// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class TabGroupSyncServiceFactoryTest {
    @Rule public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @MediumTest
    public void testSettingTestFactory() throws TimeoutException {
        TabGroupSyncService testService =
                new TabGroupSyncService() {
                    @Override
                    public void addObserver(Observer observer) {}

                    @Override
                    public void removeObserver(Observer observer) {}

                    @Override
                    public void createGroup(int groupId) {}

                    @Override
                    public void removeGroup(int groupId) {}

                    @Override
                    public void updateVisualData(
                            int tabGroupId, @NonNull String title, int color) {}

                    @Override
                    public void addTab(
                            int tabGroupId, int tabId, String title, GURL url, int position) {}

                    @Override
                    public void updateTab(
                            int tabGroupId, int tabId, String title, GURL url, int position) {}

                    @Override
                    public void removeTab(int tabGroupId, int tabId) {}

                    @Override
                    public String[] getAllGroupIds() {
                        return new String[0];
                    }

                    @Override
                    public SavedTabGroup getGroup(String syncGroupId) {
                        return null;
                    }

                    @Override
                    public SavedTabGroup getGroup(int localGroupId) {
                        return null;
                    }

                    @Override
                    public void updateLocalTabGroupId(String syncId, int localId) {}

                    @Override
                    public void updateLocalTabId(
                            int localGroupId, String syncTabId, int localTabId) {}
                };

        TabGroupSyncServiceFactory.setForTesting(testService);
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
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

        mActivityTestRule.runOnUiThread(
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
