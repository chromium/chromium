// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.test.filters.LargeTest;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SessionHeader;
import org.chromium.components.sync.protocol.SessionSpecifics;
import org.chromium.components.sync.protocol.SessionTab;
import org.chromium.components.sync.protocol.SessionWindow;
import org.chromium.components.sync.protocol.SyncEnums;
import org.chromium.components.sync.protocol.TabNavigation;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * Test suite for the open tabs (sessions) sync data type.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OpenTabsTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String TAG = "OpenTabsTest";

    private static final String OPEN_TABS_TYPE = "Sessions";

    // EmbeddedTestServer is preferred here but it can't be used. The test server
    // serves pages on localhost and Chrome doesn't sync localhost URLs as typed URLs.
    // This type of URL requires no external data connection or resources.
    private static final String URL = "data:text,OpenTabsTestURL";
    private static final String URL2 = "data:text,OpenTabsTestURL2";
    private static final String URL3 = "data:text,OpenTabsTestURL3";

    private static final String SESSION_TAG_PREFIX = "FakeSessionTag";
    private static final String FAKE_CLIENT = "FakeClient";

    // The client name for tabs generated locally will vary based on the device the test is
    // running on, so it is determined once in the setUp() method and cached here.
    private String mClientName;

    // A counter used for generating unique session tags. Resets to 0 in setUp().
    private int mSessionTagCounter;

    // A container to store OpenTabs information for data verification.
    private static class OpenTabs {
        public final String headerServerId;
        public final String headerClientTagHash;
        public final ArrayList<String> tabServerIds;
        public final ArrayList<String> tabClientTagHashes;
        public final ArrayList<String> urls;

        private OpenTabs(String headerServerId, String headerClientTagHash,
                ArrayList<String> tabServerIds, ArrayList<String> tabClientTagHashes,
                ArrayList<String> urls) {
            this.headerServerId = headerServerId;
            this.headerClientTagHash = headerClientTagHash;
            this.tabServerIds = tabServerIds;
            this.tabClientTagHashes = tabClientTagHashes;
            this.urls = urls;
        }
    }

    @Before
    public void setUp() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mClientName = getClientName();
        mSessionTagCounter = 0;
    }

    // Test syncing an open tab from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadOpenTab() {
        mSyncTestRule.loadUrl(URL);
        waitForLocalTabsForClient(mClientName, URL);
        waitForServerTabs(URL);
    }

    // Test syncing multiple open tabs from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadMultipleOpenTabs() {
        mSyncTestRule.loadUrl(URL);
        mSyncTestRule.loadUrlInNewTab(URL2);
        mSyncTestRule.loadUrlInNewTab(URL3);
        waitForLocalTabsForClient(mClientName, URL, URL2, URL3);
        waitForServerTabs(URL, URL2, URL3);
    }

    // Test syncing an open tab from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadAndCloseOpenTab() {
        mSyncTestRule.loadUrl(URL);
        // Can't have zero tabs, so we have to open two to test closing one.
        mSyncTestRule.loadUrlInNewTab(URL2);
        waitForLocalTabsForClient(mClientName, URL, URL2);
        waitForServerTabs(URL, URL2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModelSelector selector = mSyncTestRule.getActivity().getTabModelSelector();
            Assert.assertTrue(TabModelUtils.closeCurrentTab(selector.getCurrentModel()));
        });

        waitForLocalTabsForClient(mClientName, URL);
        waitForServerTabs(URL);
    }

    // Test syncing an open tab from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadOpenTab() {
        addFakeServerTabs(FAKE_CLIENT, URL);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT, URL);
    }

    // Test syncing multiple open tabs from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadMultipleOpenTabs() {
        addFakeServerTabs(FAKE_CLIENT, URL, URL2, URL3);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT, URL, URL2, URL3);
    }

    // Test syncing a tab deletion from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedOpenTab() throws Exception {
        // Add the entity to test deleting.
        addFakeServerTabs(FAKE_CLIENT, URL);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT, URL);

        // Delete on server, sync, and verify deleted locally.
        deleteServerTabsForClient(FAKE_CLIENT);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT);
    }

    // Test syncing multiple tab deletions from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadMultipleDeletedOpenTabs() throws Exception {
        // Add the entity to test deleting.
        addFakeServerTabs(FAKE_CLIENT, URL, URL2, URL3);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT, URL, URL2, URL3);

        // Delete on server, sync, and verify deleted locally.
        deleteServerTabsForClient(FAKE_CLIENT);
        SyncTestUtil.triggerSync();
        waitForLocalTabsForClient(FAKE_CLIENT);
    }

    private String makeSessionTag() {
        return SESSION_TAG_PREFIX + (mSessionTagCounter++);
    }

    private void addFakeServerTabs(String clientName, String... urls) {
        String tag = makeSessionTag();
        EntitySpecifics header = makeSessionEntity(tag, clientName, urls.length);
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(
                "" /* nonUniqueName */, tag /* clientTag */, header);
        for (int i = 0; i < urls.length; i++) {
            EntitySpecifics tab = makeTabEntity(tag, urls[i], i);
            mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(
                    "" /* nonUniqueName */, tag + " " + i /* clientTag */, tab);
        }
    }

    private SessionWindow makeSessionWindow(int numTabs) {
        SessionWindow.Builder windowBuilder =
                SessionWindow.newBuilder().setWindowId(1).setSelectedTabIndex(0);
        for (int i = 0; i < numTabs; i++) {
            windowBuilder.addTab(i + 1); // Updates |windowBuilder| internal state.
        }
        return windowBuilder.build();
    }

    private EntitySpecifics makeSessionEntity(String tag, String clientName, int numTabs) {
        SessionSpecifics session =
                SessionSpecifics.newBuilder()
                        .setSessionTag(tag)
                        .setHeader(SessionHeader.newBuilder()
                                           .setClientName(clientName)
                                           .setDeviceType(SyncEnums.DeviceType.TYPE_PHONE)
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
                        .setTab(SessionTab.newBuilder()
                                        .setTabId(id + 1)
                                        .setCurrentNavigationIndex(0)
                                        .addNavigation(TabNavigation.newBuilder()
                                                               .setVirtualUrl(url)
                                                               .build())
                                        .build())
                        .build();
        return EntitySpecifics.newBuilder().setSession(session).build();
    }

    private void deleteServerTabsForClient(String clientName) throws JSONException {
        OpenTabs openTabs = getLocalTabsForClient(clientName);
        mSyncTestRule.getFakeServerHelper().deleteEntity(
                openTabs.headerServerId, openTabs.headerClientTagHash);
        for (int i = 0; i < openTabs.tabServerIds.size(); i++) {
            mSyncTestRule.getFakeServerHelper().deleteEntity(
                    openTabs.tabServerIds.get(i), openTabs.tabClientTagHashes.get(i));
        }
    }

    private void waitForLocalTabsForClient(final String clientName, String... urls) {
        final List<String> urlList = new ArrayList<>(urls.length);
        for (String url : urls) urlList.add(url);
        mSyncTestRule.pollInstrumentationThread(
                Criteria.equals(urlList, new Callable<List<String>>() {
                    @Override
                    public List<String> call() throws Exception {
                        return getLocalTabsForClient(clientName).urls;
                    }
                }));
    }

    private void waitForServerTabs(final String... urls) {
        mSyncTestRule.pollInstrumentationThread(
                new Criteria("Expected server open tabs: " + Arrays.toString(urls)) {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            return mSyncTestRule.getFakeServerHelper().verifySessions(urls);
                        } catch (Exception e) {
                            throw new RuntimeException(e);
                        }
                    }
                });
    }

    private String getClientName() throws Exception {
        mSyncTestRule.pollInstrumentationThread(new Criteria(
                "Expected at least one tab entity to exist.") {
            @Override
            public boolean isSatisfied() {
                try {
                    return SyncTestUtil
                                   .getLocalData(mSyncTestRule.getTargetContext(), OPEN_TABS_TYPE)
                                   .size()
                            > 0;
                } catch (JSONException e) {
                    return false;
                }
            }
        });
        List<Pair<String, JSONObject>> tabEntities =
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), OPEN_TABS_TYPE);
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            if (tabEntity.second.has("header")) {
                return tabEntity.second.getJSONObject("header").getString("client_name");
            }
        }
        throw new IllegalStateException("No client name found.");
    }

    private static class HeaderInfo {
        public final String sessionTag;
        public final String headerServerId;
        public final String headerClientTagHash;
        public final List<String> tabIds;
        public HeaderInfo(String sessionTag, String headerServerId, String headerClientTagHash,
                List<String> tabIds) {
            this.sessionTag = sessionTag;
            this.headerServerId = headerServerId;
            this.headerClientTagHash = headerClientTagHash;
            this.tabIds = tabIds;
        }
    }

    // Distills the local session data into a simple data object for the given client.
    private OpenTabs getLocalTabsForClient(String clientName) throws JSONException {
        List<Pair<String, JSONObject>> tabEntities =
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), OPEN_TABS_TYPE);
        // Output lists.
        ArrayList<String> urls = new ArrayList<>();
        ArrayList<String> tabServerIds = new ArrayList<>();
        ArrayList<String> tabClientTagHashes = new ArrayList<>();
        HeaderInfo info = findHeaderInfoForClient(clientName, tabEntities);
        if (info.sessionTag == null) {
            // No client was found. Here we still want to return an empty list of urls.
            return new OpenTabs("", "", tabServerIds, tabClientTagHashes, urls);
        }
        Map<String, String> tabIdsToUrls = new HashMap<>();
        Map<String, String> tabIdsToServerIds = new HashMap<>();
        Map<String, String> tabIdsToClientTagHashes = new HashMap<>();
        findTabMappings(info.sessionTag, tabEntities, tabIdsToUrls, tabIdsToServerIds,
                tabIdsToClientTagHashes);
        // Convert the tabId list to the url list.
        for (String tabId : info.tabIds) {
            urls.add(tabIdsToUrls.get(tabId));
            tabServerIds.add(tabIdsToServerIds.get(tabId));
            tabClientTagHashes.add(tabIdsToClientTagHashes.get(tabId));
        }
        return new OpenTabs(info.headerServerId, info.headerClientTagHash, tabServerIds,
                tabClientTagHashes, urls);
    }

    // Find the header entity for clientName and extract its sessionTag and tabId list.
    private HeaderInfo findHeaderInfoForClient(
            String clientName, List<Pair<String, JSONObject>> tabEntities) throws JSONException {
        String sessionTag = null;
        String headerServerId = null;
        String headerClientTagHash = null;
        List<String> tabIds = new ArrayList<>();
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            JSONObject header = tabEntity.second.optJSONObject("header");
            if (header != null && header.getString("client_name").equals(clientName)) {
                sessionTag = tabEntity.second.getString("session_tag");
                headerClientTagHash =
                        tabEntity.second.optJSONObject("metadata").getString("client_tag_hash");
                headerServerId = tabEntity.first;
                JSONArray windows = header.getJSONArray("window");
                if (windows.length() == 0) {
                    // The client was found but there are no tabs.
                    break;
                }
                Assert.assertEquals("Only single windows are supported.", 1, windows.length());
                JSONArray tabs = windows.getJSONObject(0).getJSONArray("tab");
                for (int i = 0; i < tabs.length(); i++) {
                    tabIds.add(tabs.getString(i));
                }
                break;
            }
        }
        return new HeaderInfo(sessionTag, headerServerId, headerClientTagHash, tabIds);
    }

    // Find the associated tabs and record their tabId -> url and entityId mappings.
    private void findTabMappings(String sessionTag, List<Pair<String, JSONObject>> tabEntities,
            // Populating these maps is the output of this function.
            Map<String, String> tabIdsToUrls, Map<String, String> tabIdsToServerIds,
            Map<String, String> tabIdsToClientTagHashes) throws JSONException {
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            JSONObject json = tabEntity.second;
            if (json.has("tab") && json.getString("session_tag").equals(sessionTag)) {
                String clientTagHash = json.optJSONObject("metadata").getString("client_tag_hash");
                JSONObject tab = json.getJSONObject("tab");
                int i = tab.getInt("current_navigation_index");
                String tabId = tab.getString("tab_id");
                String url =
                        tab.getJSONArray("navigation").getJSONObject(i).getString("virtual_url");
                tabIdsToUrls.put(tabId, url);
                tabIdsToServerIds.put(tabId, tabEntity.first);
                tabIdsToClientTagHashes.put(tabId, clientTagHash);
            }
        }
    }
}
