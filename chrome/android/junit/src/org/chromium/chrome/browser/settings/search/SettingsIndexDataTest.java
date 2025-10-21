// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/**
 * Unit tests for {@link SettingsIndexData}.
 *
 * <p>These tests validate the behavior of the core data model for settings search, including
 * adding/removing entries, text normalization, searching, and scoring logic.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsIndexDataTest {

    private SettingsIndexData mIndexData;

    @Before
    public void setUp() {
        mIndexData = new SettingsIndexData();
    }

    /** Tests the basic functionality of adding and retrieving an entry. */
    @Test
    public void testAddAndGetEntry() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry(
                        "key1", "Title 1", "Header 1", "Summary 1", null, "Parent1");

        mIndexData.addEntry("key1", entry);

        assertEquals("Should retrieve the correct entry.", entry, mIndexData.getEntry("key1"));
        assertNull("Should return null for a non-existent key.", mIndexData.getEntry("key2"));
    }

    /** Tests that adding a duplicate key throws the expected exception. */
    @Test(expected = IllegalStateException.class)
    public void testAddEntry_throwsOnDuplicateKey() {
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry("key1", "Title 1", "Header 1", null, null, "P1"));
        // This second call with the same key should throw.
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry("key1", "Title 2", "Header 2", null, null, "P2"));
    }

    /** Tests the hierarchical disabling logic of removeEntry. */
    @Test
    public void testRemoveEntry_disablesTargetFragment() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry(
                        "key1", "Title 1", "Header 1", null, "FragmentToDisable", "Parent1");
        mIndexData.addEntry("key1", entry);
        assertFalse(mIndexData.isDisabledFragment("FragmentToDisable"));

        mIndexData.removeEntry("key1");

        assertNull("Entry should be removed.", mIndexData.getEntry("key1"));
        assertTrue(
                "Target fragment should now be disabled.",
                mIndexData.isDisabledFragment("FragmentToDisable"));
    }

    /** Tests that simple removal does not disable the target fragment. */
    @Test
    public void testRemoveSimpleEntry_doesNotDisableTargetFragment() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry(
                        "key1", "Title 1", "Header 1", null, "FragmentToKeep", "Parent1");
        mIndexData.addEntry("key1", entry);

        mIndexData.removeSimpleEntry("key1");

        assertNull("Entry should be removed.", mIndexData.getEntry("key1"));
        assertFalse(
                "Target fragment should NOT be disabled.",
                mIndexData.isDisabledFragment("FragmentToKeep"));
    }

    /** Tests the core search functionality, including scoring and result ordering. */
    @Test
    public void testSearch_scoringAndOrder() {
        // Setup: Add entries designed to test different scoring levels.
        mIndexData.addEntry(
                "key_summary",
                new SettingsIndexData.Entry(
                        "key_summary",
                        "Other",
                        "Header 1",
                        "Contains the word privacy",
                        null,
                        "P1"));
        mIndexData.addEntry(
                "key_title_partial",
                new SettingsIndexData.Entry(
                        "key_title_partial", "Privacy Guide", "Header 2", null, null, "P2"));
        mIndexData.addEntry(
                "key_title_exact",
                new SettingsIndexData.Entry(
                        "key_title_exact", "Privacy", "Header 2", null, null, "P3"));

        // Action: Perform the search.
        SettingsIndexData.SearchResults results = mIndexData.search("privacy");
        List<SettingsIndexData.Entry> items = results.getItems();

        // Assertions:
        assertEquals("Should find all three matching entries.", 3, items.size());
        // 1. The exact title match should have the highest score and be first.
        assertEquals("key_title_exact", items.get(0).key);
        // 2. The partial title match should be second.
        assertEquals("key_title_partial", items.get(1).key);
        // 3. The summary match should have the lowest score and be last.
        assertEquals("key_summary", items.get(2).key);
    }

    /** Tests that the text normalization (diacritic stripping) works correctly. */
    @Test
    public void testSearch_normalization_stripsDiacritics() {
        // Setup: Add an entry with an accented character.
        mIndexData.addEntry(
                "key_resume",
                new SettingsIndexData.Entry(
                        "key_resume", "Resumé Settings", "Header 1", null, null, "P1"));

        // Action: Search using the un-accented version of the word.
        SettingsIndexData.SearchResults results = mIndexData.search("resume");

        // Assertion: The search should find the correct entry.
        assertEquals("Should find one match.", 1, results.getItems().size());
        assertEquals("key_resume", results.getItems().get(0).key);
    }

    /** Tests that an empty or non-matching search returns no results. */
    @Test
    public void testSearch_noMatches() {
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry("key1", "Title", "Header", "Summary", null, "P1"));

        assertTrue(
                "Searching for a non-existent term should return empty results.",
                mIndexData.search("nonexistent").isEmpty());
        assertTrue(
                "Searching for an empty string should return empty results.",
                mIndexData.search("").isEmpty());
    }
}
