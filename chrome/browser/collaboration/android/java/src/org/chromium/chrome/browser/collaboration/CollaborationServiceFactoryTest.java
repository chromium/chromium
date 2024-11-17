// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

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
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationStatus;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.SigninStatus;
import org.chromium.components.collaboration.SyncStatus;
import org.chromium.components.data_sharing.member_role.MemberRole;

import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class CollaborationServiceFactoryTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @MediumTest
    public void testSettingTestFactory() throws TimeoutException {
        CollaborationService testService =
                new CollaborationService() {
                    @Override
                    public boolean isEmptyService() {
                        return true;
                    }

                    @Override
                    public ServiceStatus getServiceStatus() {
                        return new ServiceStatus(
                                SigninStatus.NOT_SIGNED_IN,
                                SyncStatus.NOT_SYNCING,
                                CollaborationStatus.DISABLED);
                    }

                    @Override
                    public @MemberRole int getCurrentUserRoleForGroup(String groupId) {
                        return MemberRole.UNKNOWN;
                    }
                };

        CollaborationServiceFactory.setForTesting(testService);
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        CollaborationService collaborationService =
                                CollaborationServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertTrue(collaborationService.isEmptyService());
                        Assert.assertEquals(collaborationService, testService);
                    }
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testServiceCreation_RealService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        CollaborationService collaborationService =
                                CollaborationServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertFalse(collaborationService.isEmptyService());
                    }
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testServiceCreation_EmptyService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        CollaborationService collaborationService =
                                CollaborationServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertTrue(collaborationService.isEmptyService());
                    }
                });
    }
}
