// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
    private static final String ROOT_FRAGMENT = "RootFragment";
    private SettingsIndexData mIndexData;

    @Before
    public void setUp() {
        mIndexData = new SettingsIndexData();
    }

    private void addEntry(String header, String key, String title, String summary, String frag) {
        mIndexData.addEntry(
                key,
                new SettingsIndexData.Entry.Builder(key, key, title, frag)
                        .setHeader(header)
                        .setSummary(summary)
                        .setFragment(frag)
                        .build());
    }

    /** Tests the basic functionality of adding and retrieving an entry. */
    @Test
    public void testAddAndGetEntry() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title 1", "Parent1")
                        .setHeader("Header 1")
                        .setSummary("Summary 1")
                        .build();

        mIndexData.addEntry("key1", entry);

        assertEquals("Should retrieve the correct entry.", entry, mIndexData.getEntry("key1"));
        assertNull("Should return null for a non-existent key.", mIndexData.getEntry("key2"));
    }

    /** Tests that adding a duplicate key throws the expected exception. */
    @Test(expected = IllegalStateException.class)
    public void testAddEntry_throwsOnDuplicateKey() {
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title 1", "P1")
                        .setHeader("Header 1")
                        .build());
        // This second call with the same key should throw.
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title 2", "P2")
                        .setHeader("Header 2")
                        .build());
    }

    @Test
    public void testRemoveEntry() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title 1", "Parent1")
                        .setHeader("Header 1")
                        .setFragment("FragmentToKeep")
                        .build();
        mIndexData.addEntry("key1", entry);

        mIndexData.removeEntry("key1");

        assertNull("Entry should be removed.", mIndexData.getEntry("key1"));
    }

    @Test
    public void testFinalizeIndex_prunesOrphans() {
        // Setup: A -> B -> C hierarchy.
        // A is the top-level preference on the root screen.
        mIndexData.addEntry(
                "pref_A",
                new SettingsIndexData.Entry.Builder("pref_A", "pref_A", "Title A", ROOT_FRAGMENT)
                        .setFragment("FragmentB")
                        .build());
        mIndexData.addEntry(
                "pref_B",
                new SettingsIndexData.Entry.Builder("pref_B", "pref_B", "Title B", "FragmentB")
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                "pref_C",
                new SettingsIndexData.Entry.Builder("pref_C", "pref_C", "Title C", "FragmentC")
                        .build());

        mIndexData.addChildParentLink("FragmentB", "pref_A");
        mIndexData.addChildParentLink("FragmentC", "pref_B");

        mIndexData.removeEntry("pref_A");

        mIndexData.resolveIndex(ROOT_FRAGMENT);

        // Assertions:
        assertNull("Parent link pref_A should be gone.", mIndexData.getEntry("pref_A"));
        assertNull("Orphaned child pref_B should have been pruned.", mIndexData.getEntry("pref_B"));
        assertNull(
                "Orphaned grandchild pref_C should have been pruned.",
                mIndexData.getEntry("pref_C"));
    }

    @Test
    public void testFinalizeIndex_handlesMultiParentCorrectly() {
        // Setup: A child fragment (FragmentC) is reachable from two different parents (A and B).
        mIndexData.addEntry(
                "pref_A",
                new SettingsIndexData.Entry.Builder("pref_A", "pref_A", "Title A", ROOT_FRAGMENT)
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                "pref_B",
                new SettingsIndexData.Entry.Builder("pref_B", "pref_B", "Title B", ROOT_FRAGMENT)
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                "pref_C",
                new SettingsIndexData.Entry.Builder("pref_C", "pref_C", "Title C", "FragmentC")
                        .build());

        mIndexData.addChildParentLink("FragmentC", "pref_A");
        mIndexData.addChildParentLink("FragmentC", "pref_B");

        mIndexData.removeEntry("pref_A");

        mIndexData.resolveIndex(ROOT_FRAGMENT);

        assertNull("Pruned parent pref_A should be gone.", mIndexData.getEntry("pref_A"));
        assertNotNull(
                "The remaining parent link pref_B should still exist.",
                mIndexData.getEntry("pref_B"));
        assertNotNull(
                "Child pref_C should NOT be pruned as it's still reachable.",
                mIndexData.getEntry("pref_C"));
        assertEquals(
                "Child's header should be resolved via the remaining parent B.",
                "Title B",
                mIndexData.getEntry("pref_C").header);
    }

    /** Tests the core search functionality, including scoring and result ordering. */
    @Test
    public void testSearch_scoringAndOrder() {
        // Setup: Add entries designed to test different scoring levels.
        mIndexData.addEntry(
                "key_summary",
                new SettingsIndexData.Entry.Builder("key_summary", "key_summary", "Other", "P1")
                        .setHeader("Header 1")
                        .setSummary("Contains the word privacy")
                        .build());
        mIndexData.addEntry(
                "key_title_partial",
                new SettingsIndexData.Entry.Builder(
                                "key_title_partial", "key_title_partial", "Privacy Guide", "P2")
                        .setHeader("Header 2")
                        .build());
        mIndexData.addEntry(
                "key_title_exact",
                new SettingsIndexData.Entry.Builder(
                                "key_title_exact", "key_title_exact", "Privacy", "P3")
                        .setHeader("Header 2")
                        .build());

        // Action: Perform the search.
        SettingsIndexData.SearchResults results = mIndexData.search("privacy");
        List<SettingsIndexData.Entry> items = results.getItems();

        // Assertions:
        assertEquals("Should find all three matching entries.", 3, items.size());
        // 1. The exact title match should have the highest score and be first.
        assertEquals("key_title_exact", items.get(0).id);
        // 2. The partial title match should be second.
        assertEquals("key_title_partial", items.get(1).id);
        // 3. The summary match should have the lowest score and be last.
        assertEquals("key_summary", items.get(2).id);
    }

    @Test
    public void testGroupByHeader() {
        addEntry("header1", "item12", "TitleItem12", "SummaryItem12", "P12");
        addEntry("header2", "item21", "TitleItem21", "SummaryItem21", "P21");
        addEntry("header3", "item31", "TitleItem31", "SummaryItem31", "P31");
        addEntry("header2", "item22", "TitleItem22", "SummaryItem22", "P22");
        addEntry("header1", "item11", "TitleItem11", "SummaryItem11", "P11");

        SettingsIndexData.SearchResults results = mIndexData.search("Item");
        List<SettingsIndexData.Entry> items = results.groupByHeader();
        assertEquals("header1", items.get(0).header);
        assertEquals("header1", items.get(1).header);
        assertEquals("header2", items.get(2).header);
        assertEquals("header2", items.get(3).header);
        assertEquals("header3", items.get(4).header);
    }

    /** Tests that the text normalization (diacritic stripping) works correctly. */
    @Test
    public void testSearch_normalization_stripsDiacritics() {
        // Setup: Add an entry with an accented character.
        mIndexData.addEntry(
                "key_resume",
                new SettingsIndexData.Entry.Builder(
                                "key_resume", "key_resume", "Resumé Settings", "P1")
                        .setHeader("Header 1")
                        .build());

        // Action: Search using the un-accented version of the word.
        SettingsIndexData.SearchResults results = mIndexData.search("resume");

        // Assertion: The search should find the correct entry.
        assertEquals("Should find one match.", 1, results.getItems().size());
        assertEquals("key_resume", results.getItems().get(0).id);
    }

    /** Tests that an empty or non-matching search returns no results. */
    @Test
    public void testSearch_noMatches() {
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title", "P1")
                        .setHeader("Header")
                        .setSummary("Summary")
                        .build());

        assertTrue(
                "Searching for a non-existent term should return empty results.",
                mIndexData.search("nonexistent").isEmpty());
        assertTrue(
                "Searching for an empty string should return empty results.",
                mIndexData.search("").isEmpty());
    }

    @Test
    public void testClear_removesAllEntriesAndRelationships() {
        mIndexData.addEntry(
                "key1",
                new SettingsIndexData.Entry.Builder("key1", "key1", "Title 1", "ParentFragment")
                        .build());
        mIndexData.addChildParentLink("ChildFragment", "key1");

        assertFalse(
                "Entries map should not be empty before clear.",
                mIndexData.getEntriesForTesting().isEmpty());
        assertFalse(
                "Parent-child map should not be empty before clear.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());

        mIndexData.clear();

        assertTrue(
                "Entries map should be empty after clear.",
                mIndexData.getEntriesForTesting().isEmpty());
        assertTrue(
                "Parent-child map should be empty after clear.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());
    }
}
