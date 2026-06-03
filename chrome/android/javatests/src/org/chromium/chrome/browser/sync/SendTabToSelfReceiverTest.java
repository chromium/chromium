// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SendTabToSelfSpecifics;

/** Test suite for the Send Tab To Self sync data type. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Manages sign-in state, which is global.")
@EnableFeatures({ChromeFeatureList.SEND_TAB_TO_SELF_AUTO_OPEN})
public class SendTabToSelfReceiverTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final long UNIX_TO_WINDOWS_EPOCH_SECONDS = 11644473600L;

    private static long getCurrentTimeSinceWindowsEpochMicros() {
        return (System.currentTimeMillis() + UNIX_TO_WINDOWS_EPOCH_SECONDS * 1000) * 1000;
    }

    private String mLocalCacheGuid;

    @Before
    public void setUp() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        mLocalCacheGuid = mSyncTestRule.getFakeServerHelper().getLocalCacheGuid();
        long now = getCurrentTimeSinceWindowsEpochMicros();
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity(mLocalCacheGuid, "Pixel 10", now, now);
    }

    private void injectSendTabToSelfEntity(
            String guid, String url, String title, String deviceName, long sharedTime) {
        SendTabToSelfSpecifics sttsSpecifics =
                SendTabToSelfSpecifics.newBuilder()
                        .setGuid(guid)
                        .setUrl(url)
                        .setTitle(title)
                        .setDeviceName(deviceName)
                        .setTargetDeviceSyncCacheGuid(mLocalCacheGuid)
                        .setSharedTimeUsec(sharedTime)
                        .build();

        EntitySpecifics specifics =
                EntitySpecifics.newBuilder().setSendTabToSelf(sttsSpecifics).build();

        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(guid, guid, specifics);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfAutoOpenMultipleTabs() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        injectSendTabToSelfEntity(
                "stts_test_guid_1",
                "https://www.example1.com",
                "Example 1",
                "Example Phone 1",
                now);
        injectSendTabToSelfEntity(
                "stts_test_guid_2",
                "https://www.example2.com",
                "Example 2",
                "Example Phone 2",
                now + 1000);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 3, 0);

        TabModel tabModel = mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
        Assert.assertEquals(
                0, ThreadUtils.runOnUiThreadBlocking(() -> tabModel.index()).intValue());

        Tab bgTab1 = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(1));
        Assert.assertEquals(
                "https://www.example1.com/",
                ThreadUtils.runOnUiThreadBlocking(() -> bgTab1.getUrl().getSpec()));

        Tab bgTab2 = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        Assert.assertEquals(
                "https://www.example2.com/",
                ThreadUtils.runOnUiThreadBlocking(() -> bgTab2.getUrl().getSpec()));
    }
}
