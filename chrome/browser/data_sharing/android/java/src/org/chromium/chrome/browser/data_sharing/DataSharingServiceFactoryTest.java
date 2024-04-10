// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.UserDataHost;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.data_sharing.DataSharingNetworkLoader;
import org.chromium.components.data_sharing.DataSharingService;

import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class DataSharingServiceFactoryTest {
    @Rule public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @MediumTest
    public void testSettingTestFactory() throws TimeoutException {
        DataSharingService testService =
                new DataSharingService() {
                    @Override
                    public boolean isEmptyService() {
                        return true;
                    }

                    @Override
                    public DataSharingNetworkLoader getNetworkLoader() {
                        return null;
                    }

                    @Override
                    public UserDataHost getUserDataHost() {
                        return null;
                    }
                };

        DataSharingServiceFactory.setForTesting(testService);
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        DataSharingService dataSharingService =
                                DataSharingServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertTrue(dataSharingService.isEmptyService());
                        Assert.assertEquals(dataSharingService, testService);
                    }
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testServiceCreation_RealService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        DataSharingService dataSharingService =
                                DataSharingServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertFalse(dataSharingService.isEmptyService());
                    }
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testServiceCreation_EmptyService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        DataSharingService dataSharingService =
                                DataSharingServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertTrue(dataSharingService.isEmptyService());
                    }
                });
    }
}
