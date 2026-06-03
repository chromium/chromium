// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.layouts.LayoutTestUtils.waitForLayout;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SendTabToSelfSpecifics;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.concurrent.TimeUnit;

/** Test suite for the Send Tab To Self sync data type. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Manages sign-in state, which is global.")
@EnableFeatures({ChromeFeatureList.SEND_TAB_TO_SELF_AUTO_OPEN})
public class SendTabToSelfReceiverTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.SERVICES_SYNC)
                    .setRevision(1)
                    .build();

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

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSendTabToSelfMessageBanner() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        injectSendTabToSelfEntity(
                "stts_test_guid", "https://www.example.com", "Example", "Example Phone", now);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        // Verify that the message banner is displayed.
        onView(withId(R.id.message_primary_button)).check(matches(isDisplayed()));

        mRenderTestRule.render(
                mSyncTestRule.getActivity().findViewById(R.id.message_container),
                "stts_message_banner");
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfMessageBannerClickOpensTabSwitcher() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        injectSendTabToSelfEntity(
                "stts_test_guid", "https://www.example.com", "Example", "Example Phone", now);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        // Verify that the message banner is displayed.
        onView(withId(R.id.message_primary_button)).check(matches(isDisplayed()));

        // Click on the message banner primary button.
        onView(withId(R.id.message_primary_button)).perform(click());

        // Verify that the tab switcher is opened.
        waitForLayout(mSyncTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Verify that the message banner goes away.
        onView(withId(R.id.message_primary_button)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testNoSendTabToSelfMessageBannerForExpiredEntry() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        // Set the shared time to 10 days ago, which is greater than the TTL of the STTS entry.
        long sharedTime = now - TimeUnit.DAYS.toMicros(10);
        injectSendTabToSelfEntity(
                "stts_test_guid",
                "https://www.example.com",
                "Example",
                "Example Phone",
                sharedTime);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify that the STTS entry is not opened in a new tab.
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 1, 0);

        // Verify that the message banner is not displayed.
        onView(withId(R.id.message_primary_button)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSendTabToSelfReceivedTabCardLabel() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        injectSendTabToSelfEntity(
                "stts_test_guid", "https://www.example.com", "Example", "Example Phone", now);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify that the tab is opened in the background
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        TabModel tabModel = mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
        // Verify the active tab is STILL the initial tab (index 0), proving the new tab opened in
        // the background
        Assert.assertEquals(
                0, ThreadUtils.runOnUiThreadBlocking(() -> tabModel.index()).intValue());
        Tab bgTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(1));
        Assert.assertEquals(
                "https://www.example.com/",
                ThreadUtils.runOnUiThreadBlocking(() -> bgTab.getUrl().getSpec()));

        // Open the Tab Switcher
        TabUiTestHelper.enterTabSwitcher(mSyncTestRule.getActivity());

        // Verify the tab card label is displayed and compare golden screenshot
        onView(withText("From Example Phone")).check(matches(isDisplayed()));
        mRenderTestRule.render(
                mSyncTestRule.getActivity().findViewById(R.id.tab_list_recycler_view),
                "stts_tab_card_label");
    }
}
