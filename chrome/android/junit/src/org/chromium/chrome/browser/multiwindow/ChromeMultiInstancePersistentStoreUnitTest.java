// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.junit.After;

import java.util.Set;

/**
 * Unit tests for {@link ChromeMultiInstancePersistentStore}. Focuses on the Protobuf implementation
 * for instance-specific fields.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.MULTI_INSTANCE_SHARED_PREFS_MIGRATION)
public class ChromeMultiInstancePersistentStoreUnitTest {

    private static final int INSTANCE_ID_0 = 0;
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int TASK_ID_0 = 6787;
    private static final int TASK_ID_1 = 6788;
    private static final int TASK_ID_2 = 6789;

    private static final String URL = "http://google.com";
    private static final String WINDOW_TITLE = "window1";
    private static final String TAB_TITLE = "google";

    @Before
    public void setUp() {
        ChromeMultiInstancePersistentStore.ensureInitialized();
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testReadAllInstanceIds() {
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_0, TASK_ID_0);
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_1, TASK_ID_1);
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_2, TASK_ID_2);

        Set<Integer> ids = ChromeMultiInstancePersistentStore.readAllInstanceIds();
        assertEquals(3, ids.size());
        assertTrue(ids.contains(INSTANCE_ID_0));
        assertTrue(ids.contains(INSTANCE_ID_1));
        assertTrue(ids.contains(INSTANCE_ID_2));
    }

    @Test
    public void testDeleteInstanceState() {
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(INSTANCE_ID_0, URL);
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_0, TAB_TITLE);
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_0, WINDOW_TITLE);
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_0, TASK_ID_0);
        ChromeMultiInstancePersistentStore.writeTabCount(INSTANCE_ID_0, 1, 1);
        ChromeMultiInstancePersistentStore.writeTabCountForRelaunchSync(INSTANCE_ID_0, 1);
        ChromeMultiInstancePersistentStore.writeProfileType(
                INSTANCE_ID_0, SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeIncognitoSelected(INSTANCE_ID_0, true);
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(INSTANCE_ID_0, false);
        ChromeMultiInstancePersistentStore.writeLatestPersistentStateId(
                INSTANCE_ID_0, INSTANCE_ID_1);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_0);
        ChromeMultiInstancePersistentStore.writeClosureTime(INSTANCE_ID_0);

        assertTrue(ChromeMultiInstancePersistentStore.hasInstance(INSTANCE_ID_0));

        ChromeMultiInstancePersistentStore.deleteInstanceState(INSTANCE_ID_0);

        assertFalse(ChromeMultiInstancePersistentStore.hasInstance(INSTANCE_ID_0));
    }

    @Test
    public void testLastAccessedTime() {
        // Test default value.
        assertEquals(0, ChromeMultiInstancePersistentStore.readLastAccessedTime(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_0);
        assertTrue(ChromeMultiInstancePersistentStore.readLastAccessedTime(INSTANCE_ID_0) > 0);
    }

    @Test
    public void testClosureTime() {
        // Test default value.
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeClosureTime(INSTANCE_ID_0);
        assertTrue(ChromeMultiInstancePersistentStore.readClosureTime(INSTANCE_ID_0) > 0);
    }

    @Test
    public void testReadTaskMap() {
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_0, TASK_ID_0);
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_1, TASK_ID_1);

        var taskMap = ChromeMultiInstancePersistentStore.readTaskMap();
        assertEquals(2, taskMap.size());
        assertEquals(TASK_ID_0, (int) taskMap.get(0));
        assertEquals(TASK_ID_1, (int) taskMap.get(1));
    }

    @Test
    public void testTaskId() {
        // Test default value.
        assertEquals(
                MultiInstanceManager.INVALID_TASK_ID,
                ChromeMultiInstancePersistentStore.readTaskId(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeTaskId(INSTANCE_ID_0, TASK_ID_0);
        assertEquals(TASK_ID_0, ChromeMultiInstancePersistentStore.readTaskId(INSTANCE_ID_0));

        // Verify that the value is successfully cleared.
        ChromeMultiInstancePersistentStore.removeTaskId(INSTANCE_ID_0);
        assertEquals(
                MultiInstanceManager.INVALID_TASK_ID,
                ChromeMultiInstancePersistentStore.readTaskId(INSTANCE_ID_0));
    }

    @Test
    public void testTabCounts() {
        // Test default value.
        assertEquals(0, ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_0));
        assertEquals(0, ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        int normalTabCount = 10;
        int incognitoTabCount = 5;
        ChromeMultiInstancePersistentStore.writeTabCount(
                INSTANCE_ID_0, normalTabCount, incognitoTabCount);
        assertEquals(
                normalTabCount,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_0));
        assertEquals(
                incognitoTabCount,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_0));
    }

    @Test
    public void testTabCountForRelaunch() {
        // Test default value.
        assertEquals(0, ChromeMultiInstancePersistentStore.readTabCountForRelaunch(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        int tabCount = 7;
        ChromeMultiInstancePersistentStore.writeTabCountForRelaunchSync(INSTANCE_ID_0, tabCount);
        assertEquals(
                tabCount,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(INSTANCE_ID_0));
    }

    @Test
    public void testActiveTabUrl() {
        // Test default value
        assertNull(ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(INSTANCE_ID_0, URL);
        assertEquals(URL, ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_0));
    }

    @Test
    public void testActiveTabTitle() {
        // Test default value
        assertNull(ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_0, TAB_TITLE);
        assertEquals(
                TAB_TITLE, ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_0));
    }

    @Test
    public void testCustomTitle() {
        // Test default value.
        assertNull(ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_0, WINDOW_TITLE);
        assertEquals(
                WINDOW_TITLE, ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_0));

        // Verify that the value is successfully cleared.
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_0, null);
        assertNull(ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_0));
    }

    @Test
    public void testProfileType() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);

        // Test default value.
        assertEquals(
                SupportedProfileType.UNSET,
                ChromeMultiInstancePersistentStore.readProfileType(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        @SupportedProfileType int profileType = SupportedProfileType.OFF_THE_RECORD;
        ChromeMultiInstancePersistentStore.writeProfileType(INSTANCE_ID_0, profileType);
        assertEquals(
                profileType, ChromeMultiInstancePersistentStore.readProfileType(INSTANCE_ID_0));
    }

    @Test
    public void testLatestPersistentStateId() {
        // Verify default value.
        assertFalse(
                ChromeMultiInstancePersistentStore.containsLatestPersistentStateId(INSTANCE_ID_0));
        assertEquals(
                INVALID_WINDOW_ID,
                ChromeMultiInstancePersistentStore.readLatestPersistentStateId(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        int persistentStateId = INSTANCE_ID_1;
        ChromeMultiInstancePersistentStore.writeLatestPersistentStateId(
                INSTANCE_ID_0, persistentStateId);
        assertTrue(
                ChromeMultiInstancePersistentStore.containsLatestPersistentStateId(INSTANCE_ID_0));
        assertEquals(
                persistentStateId,
                ChromeMultiInstancePersistentStore.readLatestPersistentStateId(INSTANCE_ID_0));
    }

    @Test
    public void testIncognitoSelected() {
        // Verify default value.
        assertFalse(ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeIncognitoSelected(INSTANCE_ID_0, true);
        assertTrue(ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_0));

        ChromeMultiInstancePersistentStore.writeIncognitoSelected(INSTANCE_ID_0, false);
        assertFalse(ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_0));
    }

    @Test
    public void testMarkedForDeletion() {
        // Verify default value.
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(INSTANCE_ID_0));

        // Verify that the value is successfully written to the store.
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(INSTANCE_ID_0, true);
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(INSTANCE_ID_0));

        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(INSTANCE_ID_0, false);
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(INSTANCE_ID_0));
    }
}
