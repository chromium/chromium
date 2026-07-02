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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.SendTabToSelfPersistedTabData.SendTabToSelfPersistedTabDataProto;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.PersistedTabDataStorage;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tab.state.Serializer;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SendTabToSelfSpecifics;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.nio.ByteBuffer;
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
        PersistedTabDataConfiguration.setUseTestConfig(true);
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
        waitForLayout(mSyncTestRule.getActivity().getLayoutManager(), LayoutType.HUB);

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

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfLabelRemovalOnInteraction() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        injectSendTabToSelfEntity(
                "stts_test_guid", "https://www.example.com", "Example", "Example Phone", now);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        // Open the Tab Switcher
        TabUiTestHelper.enterTabSwitcher(mSyncTestRule.getActivity());
        onView(withText("From Example Phone")).check(matches(isDisplayed()));

        // Click on the STTS tab card to select/interact with it
        TabUiTestHelper.clickNthCardFromTabSwitcher(mSyncTestRule.getActivity(), 1);
        waitForLayout(mSyncTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);

        // Re-open the Tab Switcher to verify the label is gone
        TabUiTestHelper.enterTabSwitcher(mSyncTestRule.getActivity());
        onView(withText("From Example Phone")).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfTabCardLabelLoadsFromPersistentStorage() throws Exception {
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 1, 0);
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mSyncTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(false)
                                        .getTabAt(0));

        // Explicitly save a label data for `tab` in MockPersistedTabDataStorage.
        PersistedTabDataStorage storage =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> PersistedTabDataConfiguration.TEST_CONFIG.getStorage());
        String deviceName = "Example Phone";
        String guid = "stts_test_guid";
        SendTabToSelfPersistedTabDataProto proto =
                SendTabToSelfPersistedTabDataProto.newBuilder()
                        .setGuid(guid)
                        .setSenderDeviceName(deviceName)
                        .setAdditionTimestampMs(System.currentTimeMillis())
                        .build();
        ByteBuffer byteBuffer = proto.toByteString().asReadOnlyByteBuffer();
        Serializer<ByteBuffer> serializer = () -> byteBuffer;
        storage.save(
                tab.getId(),
                PersistedTabDataConfiguration.SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA.getId(),
                serializer);

        // Open the Tab Switcher.
        TabUiTestHelper.enterTabSwitcher(mSyncTestRule.getActivity());

        // Verify the card label is displayed.
        onView(withText("From Example Phone")).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfPersistedData() throws Exception {
        long startTime = System.currentTimeMillis();
        long now = getCurrentTimeSinceWindowsEpochMicros();
        String guid = "stts_test_guid";
        String deviceName = "Example Phone";
        injectSendTabToSelfEntity(guid, "https://www.example.com", "Example", deviceName, now);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify that the tab is opened in the background
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        TabModel tabModel = mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
        Tab bgTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(1));

        // Retrieve the persisted data
        SendTabToSelfTabCardLabelData data =
                ThreadUtils.runOnUiThreadBlocking(() -> SendTabToSelfTabCardLabelData.get(bgTab));

        Assert.assertNotNull(data);
        // Verify all fields of the persisted data
        Assert.assertEquals(
                guid, ThreadUtils.runOnUiThreadBlocking(() -> data.getGuidForTesting()));
        Assert.assertEquals(
                deviceName,
                ThreadUtils.runOnUiThreadBlocking(() -> data.getSenderDeviceNameForTesting()));

        long additionTimestamp =
                ThreadUtils.runOnUiThreadBlocking(() -> data.getAdditionTimestampMsForTesting());
        Assert.assertTrue(
                "Addition timestamp should be after test start", additionTimestamp >= startTime);
        Assert.assertTrue(
                "Addition timestamp should be before or equal to current time",
                additionTimestamp <= System.currentTimeMillis());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfActivationLoggingAfterRestart() throws Exception {
        long startTime = System.currentTimeMillis();
        long now = getCurrentTimeSinceWindowsEpochMicros();
        String guid = "stts_test_guid";
        String deviceName = "Example Phone";

        // Inject the entity. This will trigger auto-open in the background.
        injectSendTabToSelfEntity(guid, "https://www.example.com", "Example", deviceName, now);

        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify that the tab is opened in the background (we now have 2 tabs).
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        TabModel tabModel = mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
        Tab bgTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(1));

        // Verify data is initially loaded in memory.
        SendTabToSelfTabCardLabelData data =
                ThreadUtils.runOnUiThreadBlocking(() -> SendTabToSelfTabCardLabelData.get(bgTab));
        Assert.assertNotNull(data);

        // Simulate restart by evicting the Java-side in-memory data.
        // This removes it from UserDataHost and calls destroy() (unregistering the observer).
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        bgTab.getUserDataHost()
                                .removeUserData(SendTabToSelfTabCardLabelData.class)
                                .destroy());

        // Verify it is gone from memory.
        Assert.assertNull(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                bgTab.getUserDataHost()
                                        .getUserData(SendTabToSelfTabCardLabelData.class)));

        // Start watching for the expected histograms.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Sharing.SendTabToSelf.TimeOpenedToActivated")
                        // ShareActivatedEntryPoint.TAB_STRIP is 4
                        .expectIntRecord("Sharing.SendTabToSelf.ActivatedEntryPoint", 4)
                        .build();

        // Open the Tab Switcher. This should trigger reloading the data from disk.
        TabUiTestHelper.enterTabSwitcher(mSyncTestRule.getActivity());

        // Verify the card label is displayed (meaning it was loaded from disk).
        onView(withText("From Example Phone")).check(matches(isDisplayed()));

        // Click on the tab card to activate it.
        // Note: The auto-opened tab is at index 1. Index 0 is the default tab.
        TabUiTestHelper.clickNthCardFromTabSwitcher(mSyncTestRule.getActivity(), 1);
        waitForLayout(mSyncTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);

        // Verify that the histograms were logged.
        watcher.assertExpected();

        // Verify that the data was removed after activation.
        Assert.assertNull(
                ThreadUtils.runOnUiThreadBlocking(() -> SendTabToSelfTabCardLabelData.get(bgTab)));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSendTabToSelfClosedWithoutActivationLogging() throws Exception {
        long now = getCurrentTimeSinceWindowsEpochMicros();
        String guid = "stts_test_guid";
        String deviceName = "Example Phone";

        // Inject the entity. This will trigger auto-open in the background.
        injectSendTabToSelfEntity(guid, "https://www.example.com", "Example", deviceName, now);

        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify that the tab is opened in the background (we now have 2 tabs).
        TabUiTestHelper.verifyTabModelTabCount(mSyncTestRule.getActivity(), 2, 0);

        TabModel tabModel = mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
        Tab bgTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(1));

        // Verify data is initially loaded in memory.
        SendTabToSelfTabCardLabelData data =
                ThreadUtils.runOnUiThreadBlocking(() -> SendTabToSelfTabCardLabelData.get(bgTab));
        Assert.assertNotNull(data);

        // Start watching for the expected histograms.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        // ShareActivatedEntryPoint.TAB_OR_BROWSER_CLOSED_WITHOUT_ACTIVATION is 6
                        .expectIntRecord("Sharing.SendTabToSelf.ActivatedEntryPoint", 6)
                        .build();

        // Close the tab.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(bgTab).allowUndo(false).build(),
                                        /* allowDialog= */ false));

        // Verify that the histograms were logged.
        watcher.assertExpected();
    }
}
