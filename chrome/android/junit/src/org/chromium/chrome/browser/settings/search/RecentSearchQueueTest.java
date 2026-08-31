// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.json.JSONArray;
import org.json.JSONException;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Unit tests for {@link RecentSearchQueue}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class RecentSearchQueueTest {
    private static final String PREF_KEY = ChromePreferenceKeys.SETTINGS_RECENT_SEARCH_ENTRIES;

    @SuppressWarnings("NullAway.Init")
    private SharedPreferencesManager mPreferences;

    @SuppressWarnings("NullAway.Init")
    private RecentSearchQueue mQueue;

    @Before
    public void setUp() {
        resetQueue();
        mPreferences = ChromeSharedPreferences.getInstance();
        mQueue = RecentSearchQueue.getInstance();
    }

    @After
    public void tearDown() {
        resetQueue();
    }

    @Test
    public void testUserSelectionsPersistOnlyWhenFlushed() throws JSONException {
        mQueue.add(createEntry("one"));
        mQueue.add(createEntry("two"));

        assertFalse(mPreferences.contains(PREF_KEY));

        mQueue.flushIfDirty();

        JSONArray entries = new JSONArray(mPreferences.readString(PREF_KEY, ""));
        assertEquals(2, entries.length());
        assertEquals("one", entries.getJSONObject(0).getString("key"));
        assertEquals("two", entries.getJSONObject(1).getString("key"));

        assertTrue(mPreferences.removeKeySync(PREF_KEY));
        mQueue.flushIfDirty();
        assertFalse(mPreferences.contains(PREF_KEY));
    }

    @Test
    public void testRestoreDoesNotMarkQueueDirty() {
        mQueue.add(createEntry("restored"));
        mQueue.flushIfDirty();
        mQueue.persistToDiskAndReset();

        mQueue = RecentSearchQueue.getInstance();
        mQueue.restoreFromDisk();
        assertEquals(1, mQueue.size());

        assertTrue(mPreferences.removeKeySync(PREF_KEY));
        mQueue.flushIfDirty();
        assertFalse(mPreferences.contains(PREF_KEY));
    }

    @Test
    public void testClearAndPersistUpdatesPreference() {
        mQueue.add(createEntry("deleted"));
        mQueue.flushIfDirty();

        mQueue.clearAndPersist();

        assertTrue(mQueue.isEmpty());
        assertEquals("[]", mPreferences.readString(PREF_KEY, ""));

        mQueue.persistToDiskAndReset();
        mQueue = RecentSearchQueue.getInstance();
        mQueue.restoreFromDisk();
        assertTrue(mQueue.isEmpty());
    }

    @Test
    public void testDeleteDiskDataClearsPendingMutation() {
        mQueue.add(createEntry("deleted"));

        RecentSearchQueue.deleteDiskData();
        mQueue.flushIfDirty();

        assertTrue(mQueue.isEmpty());
        assertFalse(mPreferences.contains(PREF_KEY));
    }

    @Test
    public void testPersistToDiskAndResetFlushesPendingMutation() {
        mQueue.add(createEntry("final"));

        mQueue.persistToDiskAndReset();

        mQueue = RecentSearchQueue.getInstance();
        mQueue.restoreFromDisk();
        assertEquals(1, mQueue.size());
        assertTrue(mQueue.containsKey("final"));
    }

    private static SettingsIndexData.Entry createEntry(String key) {
        return new SettingsIndexData.Entry.Builder("id_" + key, key, "Title " + key, "Parent")
                .build();
    }

    private static void resetQueue() {
        RecentSearchQueue.deleteDiskData();
        RecentSearchQueue.getInstance().persistToDiskAndReset();
        RecentSearchQueue.deleteDiskData();
    }
}
