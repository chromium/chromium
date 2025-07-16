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

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.data_sharing.TestDataSharingService;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class DataSharingServiceFactoryTest {

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @MediumTest
    public void testSettingTestFactory() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DataSharingService testService = new TestDataSharingService();
                    DataSharingServiceFactory.setForTesting(testService);
                });
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DataSharingService dataSharingService =
                            DataSharingServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    Assert.assertTrue(dataSharingService.isEmptyService());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    // TODO(b/343541441) : Fix this test with `chrome_internal_flag`.
    public void testServiceCreation_RealService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();

        CountDownLatch countDownLatch = new CountDownLatch(6); // 6 method calls to wait for.

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    void callbackReceived() {
                        countDownLatch.countDown();
                    }

                    @Override
                    public void run() {
                        DataSharingService dataSharingService =
                                DataSharingServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertFalse(dataSharingService.isEmptyService());

                        // TODO(ssid): Add tests with SDK delegate once available.
                        dataSharingService.readGroup(
                                "bad_id",
                                result -> {
                                    Assert.assertTrue(result.groupData == null);
                                    Assert.assertEquals(
                                            PeopleGroupActionFailure.TRANSIENT_FAILURE,
                                            result.actionFailure);
                                    callbackReceived();
                                });
                        dataSharingService.createGroup(
                                "bad_name",
                                result -> {
                                    Assert.assertTrue(result.groupData == null);
                                    Assert.assertEquals(
                                            PeopleGroupActionFailure.TRANSIENT_FAILURE,
                                            result.actionFailure);
                                    callbackReceived();
                                });
                        dataSharingService.inviteMember(
                                "bad_id",
                                "bad_email",
                                result -> {
                                    Assert.assertEquals(
                                            PeopleGroupActionOutcome.TRANSIENT_FAILURE,
                                            result.intValue());
                                    callbackReceived();
                                });
                        dataSharingService.addMember(
                                "bad_id",
                                "bad_token",
                                result -> {
                                    Assert.assertEquals(
                                            PeopleGroupActionOutcome.TRANSIENT_FAILURE,
                                            result.intValue());
                                    callbackReceived();
                                });
                        dataSharingService.removeMember(
                                "bad_id",
                                "bad_email",
                                result -> {
                                    Assert.assertEquals(
                                            PeopleGroupActionOutcome.TRANSIENT_FAILURE,
                                            result.intValue());
                                    callbackReceived();
                                });
                        dataSharingService.ensureGroupVisibility(
                                "bad_id",
                                result -> {
                                    Assert.assertTrue(result.groupData == null);
                                    Assert.assertEquals(
                                            PeopleGroupActionFailure.TRANSIENT_FAILURE,
                                            result.actionFailure);
                                    callbackReceived();
                                });
                    }
                });

        // Wait for all the callbacks to return.
        try {
            countDownLatch.await();
        } catch (InterruptedException e) {
            Assert.assertTrue(false);
        }
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.DATA_SHARING_JOIN_ONLY})
    public void testServiceCreation_EmptyService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
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
