// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
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
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.collaboration.CollaborationStatus;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.SigninStatus;
import org.chromium.components.collaboration.SyncStatus;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.url.GURL;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = PER_CLASS)
public class CollaborationServiceFactoryTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

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
                    public void startJoinFlow(CollaborationControllerDelegate delegate, GURL url) {}

                    @Override
                    public void startShareOrManageFlow(
                            CollaborationControllerDelegate delegate,
                            EitherGroupId eitherId,
                            @CollaborationServiceShareOrManageEntryPoint int entry) {}

                    @Override
                    public void startLeaveOrDeleteFlow(
                            CollaborationControllerDelegate delegate,
                            EitherGroupId eitherId,
                            @CollaborationServiceLeaveOrDeleteEntryPoint int entry) {}

                    @Override
                    public ServiceStatus getServiceStatus() {
                        return new ServiceStatus(
                                SigninStatus.NOT_SIGNED_IN,
                                SyncStatus.NOT_SYNCING,
                                CollaborationStatus.DISABLED);
                    }

                    @Override
                    public @MemberRole int getCurrentUserRoleForGroup(String collaborationId) {
                        return MemberRole.UNKNOWN;
                    }

                    @Override
                    public @Nullable GroupData getGroupData(String collaborationId) {
                        return null;
                    }

                    @Override
                    public void leaveGroup(String groupId, Callback<Boolean> callback) {
                        callback.onResult(false);
                    }

                    @Override
                    public void deleteGroup(String groupId, Callback<Boolean> callback) {
                        callback.onResult(false);
                    }

                    @Override
                    public void addObserver(Observer observer) {}

                    @Override
                    public void removeObserver(Observer observer) {}
                };

        CollaborationServiceFactory.setForTesting(testService);
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();
        CountDownLatch countDownLatch = new CountDownLatch(2); // 2 method calls to wait for.

        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    void callbackReceived() {
                        countDownLatch.countDown();
                    }

                    @Override
                    public void run() {
                        CollaborationService collaborationService =
                                CollaborationServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile());
                        Assert.assertTrue(collaborationService.isEmptyService());
                        Assert.assertEquals(collaborationService, testService);

                        collaborationService.leaveGroup(
                                "bad_id",
                                result -> {
                                    Assert.assertFalse(result);
                                    callbackReceived();
                                });
                        collaborationService.deleteGroup(
                                "bad_id",
                                result -> {
                                    Assert.assertFalse(result);
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
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testServiceCreation_RealService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();

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
    @DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.DATA_SHARING_JOIN_ONLY})
    public void testServiceCreation_EmptyService() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startOnBlankPage();

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
