// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.protobuf.InvalidProtocolBufferException;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.send_tab_to_self.OtherDevicesShortcutController;
import org.chromium.chrome.browser.share.send_tab_to_self.OtherDevicesShortcutControllerFactory;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfShareTargetActivity;
import org.chromium.chrome.browser.share.send_tab_to_self.TargetDeviceInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SessionHeader;
import org.chromium.components.sync.protocol.SessionSpecifics;
import org.chromium.components.sync.protocol.SessionTab;
import org.chromium.components.sync.protocol.SessionWindow;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.SyncEnums;
import org.chromium.components.sync.protocol.TabNavigation;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

/** Integration tests for OtherDevicesShortcutController. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "sync-short-nudge-delay-for-test"
})
@DoNotBatch(reason = "Manages sign-in state, which is global.")
@MinAndroidSdkLevel(VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
public class OtherDevicesShortcutControllerIntegrationTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String REMOTE_GUID = "remote_guid_12345";
    private static final String REMOTE_NAME = "Remote Laptop";

    private ShortcutManager mShortcutManager;

    @Before
    public void setUp() {
        mSyncTestRule.setUpAccountAndEnableHistorySync();
        mShortcutManager =
                ContextUtils.getApplicationContext().getSystemService(ShortcutManager.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OtherDevicesShortcutControllerFactory.getForProfile(
                            ProfileManager.getLastUsedRegularProfile());
                });
    }

    @After
    public void tearDown() {
        List<String> idsToRemove =
                mShortcutManager.getDynamicShortcuts().stream()
                        .map(ShortcutInfo::getId)
                        .filter(
                                id ->
                                        id.startsWith(
                                                OtherDevicesShortcutController.SHORTCUT_ID_PREFIX))
                        .collect(Collectors.toList());
        if (!idsToRemove.isEmpty()) {
            mShortcutManager.removeDynamicShortcuts(idsToRemove);
        }
    }

    private void injectDeviceInfo(String guid, String name, long lastUpdatedMs) {
        mSyncTestRule
                .getFakeServerHelper()
                .injectDeviceInfoEntity(guid, name, lastUpdatedMs, lastUpdatedMs);
    }

    private List<SyncEntity> getDeviceInfoEntities() {
        try {
            List<SyncEntity> entities =
                    mSyncTestRule
                            .getFakeServerHelper()
                            .getSyncEntitiesByDataType(DataType.DEVICE_INFO);
            return entities;
        } catch (InvalidProtocolBufferException e) {
            Assert.fail(e.toString());
            return new ArrayList<>();
        }
    }

    private boolean deleteServerDeviceInfo(String guid) {
        for (SyncEntity entity : getDeviceInfoEntities()) {
            EntitySpecifics specifics = entity.getSpecifics();
            if (specifics.hasDeviceInfo()
                    && guid.equals(specifics.getDeviceInfo().getCacheGuid())) {
                mSyncTestRule
                        .getFakeServerHelper()
                        .deleteEntity(entity.getIdString(), entity.getClientTagHash());
                return true;
            }
        }
        return false;
    }

    private void injectSession(String deviceGuid, String clientName, String... urls) {
        EntitySpecifics header = makeSessionHeaderEntity(deviceGuid, clientName, urls.length);
        mSyncTestRule
                .getFakeServerHelper()
                .injectUniqueClientEntity(
                        /* nonUniqueName= */ "", /* clientTag= */ deviceGuid, header);
        for (int i = 0; i < urls.length; i++) {
            EntitySpecifics tab = makeTabEntity(deviceGuid, urls[i], i);
            mSyncTestRule
                    .getFakeServerHelper()
                    .injectUniqueClientEntity(
                            /* nonUniqueName= */ "", /* clientTag= */ deviceGuid + " " + i, tab);
        }
    }

    private SessionWindow makeSessionWindow(int numTabs) {
        SessionWindow.Builder windowBuilder =
                SessionWindow.newBuilder().setWindowId(1).setSelectedTabIndex(0);
        for (int i = 0; i < numTabs; i++) {
            windowBuilder.addTab(i + 1);
        }
        return windowBuilder.build();
    }

    private EntitySpecifics makeSessionHeaderEntity(String tag, String clientName, int numTabs) {
        SessionSpecifics session =
                SessionSpecifics.newBuilder()
                        .setSessionTag(tag)
                        .setHeader(
                                SessionHeader.newBuilder()
                                        .setClientName(clientName)
                                        .setDeviceType(SyncEnums.DeviceType.TYPE_CROS)
                                        .addWindow(makeSessionWindow(numTabs))
                                        .build())
                        .build();
        return EntitySpecifics.newBuilder().setSession(session).build();
    }

    private EntitySpecifics makeTabEntity(String tag, String url, int id) {
        SessionSpecifics session =
                SessionSpecifics.newBuilder()
                        .setSessionTag(tag)
                        .setTabNodeId(id)
                        .setTab(
                                SessionTab.newBuilder()
                                        .setTabId(id + 1)
                                        .setCurrentNavigationIndex(0)
                                        .addNavigation(
                                                TabNavigation.newBuilder()
                                                        .setVirtualUrl(url)
                                                        .build())
                                        .build())
                        .build();
        return EntitySpecifics.newBuilder().setSession(session).build();
    }

    private List<ShortcutInfo> getSttsShortcuts() {
        return mShortcutManager.getDynamicShortcuts().stream()
                .filter(
                        s ->
                                s.getId()
                                        .startsWith(
                                                OtherDevicesShortcutController.SHORTCUT_ID_PREFIX))
                .collect(Collectors.toList());
    }

    private List<String> getSttsShortcutGuidsInRankOrder() {
        return mShortcutManager.getDynamicShortcuts().stream()
                .filter(
                        s ->
                                s.getId()
                                        .startsWith(
                                                OtherDevicesShortcutController.SHORTCUT_ID_PREFIX))
                .sorted((s1, s2) -> Integer.compare(s1.getRank(), s2.getRank()))
                .map(
                        s ->
                                s.getId()
                                        .substring(
                                                OtherDevicesShortcutController.SHORTCUT_ID_PREFIX
                                                        .length()))
                .collect(Collectors.toList());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testShortcutsUpdated() {
        // Setup: Inject a too-old DeviceInfo.
        final long fifteenDaysAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(15);
        injectDeviceInfo(REMOTE_GUID, REMOTE_NAME, fifteenDaysAgo);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify no shortcuts are created since the DeviceInfo is too old.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            0,
                            SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(
                                            ProfileManager.getLastUsedRegularProfile())
                                    .size());
                    Assert.assertTrue(getSttsShortcuts().isEmpty());
                });

        // Inject a foreign session for the same device (which will have modification time = Now).
        injectSession(REMOTE_GUID, REMOTE_NAME, "https://www.example.com");
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify the device is now considered active.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    List<TargetDeviceInfo> devices =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(
                                                    ProfileManager.getLastUsedRegularProfile()));
                    Criteria.checkThat(devices.size(), Matchers.is(1));
                    Criteria.checkThat(devices.get(0).cacheGuid, Matchers.is(REMOTE_GUID));
                });

        // Verify the shortcut has been created.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    List<ShortcutInfo> shortcuts = getSttsShortcuts();
                    Criteria.checkThat(shortcuts.size(), Matchers.is(1));
                    Criteria.checkThat(
                            shortcuts.get(0).getId(),
                            Matchers.is(
                                    OtherDevicesShortcutController.SHORTCUT_ID_PREFIX
                                            + REMOTE_GUID));
                    Criteria.checkThat(shortcuts.get(0).getShortLabel(), Matchers.is(REMOTE_NAME));
                    Criteria.checkThat(
                            shortcuts.get(0).getLongLabel(),
                            Matchers.is(
                                    TestAccounts.ACCOUNT1.getGivenName() + " • " + REMOTE_NAME));
                });

        // Update the DeviceInfo entity. This should update the shortcut too.
        injectDeviceInfo(REMOTE_GUID, "New Name", System.currentTimeMillis());
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    List<ShortcutInfo> shortcuts = getSttsShortcuts();
                    Criteria.checkThat(shortcuts.size(), Matchers.is(1));
                    Criteria.checkThat(shortcuts.get(0).getShortLabel(), Matchers.is("New Name"));
                    Criteria.checkThat(
                            shortcuts.get(0).getLongLabel(),
                            Matchers.is(TestAccounts.ACCOUNT1.getGivenName() + " • New Name"));
                });

        // Delete the device info. This should remove the shortcut.
        Assert.assertTrue(deleteServerDeviceInfo(REMOTE_GUID));
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getSttsShortcuts().isEmpty(), Matchers.is(true));
                });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTopTwoDevicesSorting() {
        // Setup: Inject 3 devices.
        final long now = System.currentTimeMillis();
        final long oneDayAgo = now - TimeUnit.DAYS.toMillis(1);
        final long twoDaysAgo = now - TimeUnit.DAYS.toMillis(2);
        final long threeDaysAgo = now - TimeUnit.DAYS.toMillis(3);

        injectDeviceInfo("guid_A", "Laptop A", threeDaysAgo);
        injectDeviceInfo("guid_B", "Laptop B", twoDaysAgo);
        injectDeviceInfo("guid_C", "Laptop C", oneDayAgo);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify shortcuts are created for C and B (the two most recent devices) in rank order.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    List<String> guids = getSttsShortcutGuidsInRankOrder();
                    Criteria.checkThat(guids, Matchers.contains("guid_C", "guid_B"));
                });

        // Inject a session for the oldest device A (making it active *now*). The shortcuts should
        // get updated to A and C (dropping B, which is now the oldest).
        injectSession("guid_A", "Laptop A", "https://www.example.com");
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    List<String> guids = getSttsShortcutGuidsInRankOrder();
                    Criteria.checkThat(guids, Matchers.contains("guid_A", "guid_C"));
                });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDirectShareShortcutSendsTab() {
        // Setup: Inject a target device.
        injectDeviceInfo(REMOTE_GUID, REMOTE_NAME, System.currentTimeMillis());
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify a shortcut is created.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getSttsShortcuts().size(), Matchers.is(1));
                });

        // Simulate selecting the DirectShare target.
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setClass(context, SendTabToSelfShareTargetActivity.class);
        intent.setType("text/plain");
        intent.putExtra(
                Intent.EXTRA_SHORTCUT_ID,
                OtherDevicesShortcutController.SHORTCUT_ID_PREFIX + REMOTE_GUID);
        intent.putExtra(Intent.EXTRA_TEXT, "https://www.google.com");
        intent.putExtra(Intent.EXTRA_SUBJECT, "Google");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        context.startActivity(intent);

        // Verify the tab is added to the FakeServer.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        List<SyncEntity> entities =
                                mSyncTestRule
                                        .getFakeServerHelper()
                                        .getSyncEntitiesByDataType(DataType.SEND_TAB_TO_SELF);
                        Criteria.checkThat(entities.size(), Matchers.is(1));
                        EntitySpecifics specifics = entities.get(0).getSpecifics();
                        Criteria.checkThat(specifics.hasSendTabToSelf(), Matchers.is(true));
                        Criteria.checkThat(
                                specifics.getSendTabToSelf().getUrl(),
                                Matchers.is("https://www.google.com/"));
                        Criteria.checkThat(
                                specifics.getSendTabToSelf().getTitle(), Matchers.is("Google"));
                        Criteria.checkThat(
                                specifics.getSendTabToSelf().getTargetDeviceSyncCacheGuid(),
                                Matchers.is(REMOTE_GUID));
                    } catch (InvalidProtocolBufferException e) {
                        throw new RuntimeException(e);
                    }
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testLauncherShortcutOpensRecentTabs() {
        // Setup: Inject a target device.
        injectDeviceInfo(REMOTE_GUID, REMOTE_NAME, System.currentTimeMillis());
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        // Verify a shortcut is created.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getSttsShortcuts().size(), Matchers.is(1));
                });

        // Simulate selecting the launcher shortcut.
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(OtherDevicesShortcutController.ACTION_OPEN_RECENT_TABS);
        intent.setClassName(context, BrowserIntentUtils.LAUNCHER_SHORTCUT_ACTIVITY_CLASS_NAME);
        intent.putExtra(OtherDevicesShortcutController.EXTRA_DEVICE_GUID, REMOTE_GUID);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        context.startActivity(intent);

        // Verify Chrome has opened the Recent Tabs page for this device.
        String expectedUrl = String.format("%s#%s", UrlConstants.RECENT_TABS_URL, REMOTE_GUID);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Tab tab =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> mSyncTestRule.getActivity().getActivityTab());
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    String url = ChromeTabUtils.getUrlStringOnUiThread(tab);
                    Criteria.checkThat(url, Matchers.is(expectedUrl));
                });

        // Clean up: close the created "Recent Tabs" tab.
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mSyncTestRule.getActivity());
    }
}
