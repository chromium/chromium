// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.text.Normalizer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Data from Preferences used for settings search. This is a collection of data to be indexed. */
@NullMarked
public class SettingsIndexData {

    /* Scores given to the entries having matches with a query */
    public static final int EXACT_TITLE_MATCH = 10;
    public static final int PARTIAL_TITLE_MATCH = 5;
    public static final int PARTIAL_SUMMARY_MATCH = 3;

    private final Map<String, Entry> mEntries = new LinkedHashMap<>();
    // Map from a child fragment's class name to the list of preference keys that can link to it.
    private final Map<String, List<String>> mChildFragmentToParentKeys = new HashMap<>();

    /**
     * Normalizes a string for search by converting it to lowercase and stripping diacritics.
     *
     * @param input The string to normalize.
     * @return The normalized string, or null if the input was null.
     */
    @Nullable
    private static String normalizeString(@Nullable String input) {
        if (input == null) return null;

        // 1. Decompose characters into base letters and combining accent marks.
        String decomposed = Normalizer.normalize(input, Normalizer.Form.NFD);
        // 2. Remove the combining accent marks using a regular expression.
        String stripped = decomposed.replaceAll("\\p{InCombiningDiacriticalMarks}+", "");
        // 3. Convert to lowercase for case-insensitive matching.
        return stripped.toLowerCase(Locale.getDefault());
    }

    /**
     * An immutable data model representing a single searchable preference.
     *
     * <p>This is a value object that holds all the necessary information for both indexing and
     * displaying a search result. Its public fields are final to guarantee immutability, ensuring
     * that its state cannot be changed after creation.
     */
    public static class Entry {
        /** The entry's globally unique id. */
        public final String id;

        /** The original key defined for Preference/Fragment. */
        public final String key;

        /** Title of Preference/Fragment. */
        public final @Nullable String title;

        /** Summary/description of Preference/Fragment. */
        public final @Nullable String summary;

        /** Package path name if the entry is Fragment. Otherwise {@code null}. */
        public final @Nullable String fragment;

        /**
         * Top-level setting entry where this entry belongs, such as Privacy and security, Payment,
         * Languages.
         */
        public final @Nullable String header;

        /** Package path name of the immediate parent Fragment of this entry. */
        public final String parentFragment;

        /** Extra arguments needed to launch a pref. */
        public final Bundle extras;

        private final @Nullable String mTitleNormalized;
        private final @Nullable String mSummaryNormalized;

        private Entry(
                String id,
                String key,
                @Nullable String title,
                @Nullable String header,
                @Nullable String summary,
                @Nullable String fragment,
                Bundle extras,
                String parentFragment,
                @Nullable String titleNormalized,
                @Nullable String summaryNormalized) {
            this.id = id;
            this.key = key;
            this.title = title;
            this.header = header;
            this.summary = summary;
            this.fragment = fragment;
            this.extras = extras;
            this.parentFragment = parentFragment;
            mTitleNormalized = titleNormalized;
            mSummaryNormalized = summaryNormalized;
        }

        /**
         * A builder for creating immutable {@link Entry} objects. For future modifications, please
         * be aware of the normalized fields.
         */
        public static class Builder {
            private final String mId;
            private final String mKey;
            private @Nullable String mTitle;
            private @Nullable String mHeader;
            private @Nullable String mSummary;
            private @Nullable String mFragment;
            private Bundle mExtras;
            private final String mParentFragment;

            /**
             * Constructs a builder with the minimum required fields for creating a new {@link
             * Entry}.
             *
             * @param id The unique id of the preference.
             * @param key The key of the preference.
             * @param title The title of the preference.
             * @param parentFragment The class name of the fragment containing this preference.
             */
            public Builder(String id, String key, @Nullable String title, String parentFragment) {
                mId = id;
                mKey = key;
                mTitle = title;
                mParentFragment = parentFragment;
                mExtras = new Bundle();
            }

            /**
             * Constructs a builder by copying the state from an existing {@link Entry}.
             *
             * @param original The original {@link Entry} to copy.
             */
            public Builder(Entry original) {
                mId = original.id;
                mKey = original.key;
                mTitle = original.title;
                mHeader = original.header;
                mSummary = original.summary;
                mFragment = original.fragment;
                mExtras = original.extras;
                mParentFragment = original.parentFragment;
            }

            public Builder setTitle(@Nullable String title) {
                mTitle = title;
                return this;
            }

            public Builder setHeader(@Nullable String header) {
                mHeader = header;
                return this;
            }

            public Builder setSummary(@Nullable String summary) {
                mSummary = summary;
                return this;
            }

            public Builder setFragment(@Nullable String fragment) {
                mFragment = fragment;
                return this;
            }

            public Builder setArguments(Bundle extras) {
                mExtras = extras;
                return this;
            }

            /**
             * Creates an {@link Entry} object.
             *
             * @return A new, immutable {@link Entry} instance.
             */
            public Entry build() {
                String titleNormalized = normalizeString(mTitle);
                String summaryNormalized = normalizeString(mSummary);

                return new Entry(
                        mId,
                        mKey,
                        mTitle,
                        mHeader,
                        mSummary,
                        mFragment,
                        mExtras,
                        mParentFragment,
                        titleNormalized,
                        summaryNormalized);
            }
        }
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param key The unique key of the preference. This is used as the primary identifier in the
     *     internal map.
     * @param entry The {@link Entry} object containing all the data for this preference.
     * @throws IllegalStateException If a preference with the same key already exists in the index.
     */
    public void addEntry(String key, Entry entry) {
        if (mEntries.containsKey(key)) {
            throw new IllegalStateException("Duplicate preference key found: " + key);
        }
        mEntries.put(key, entry);
    }

    @Nullable
    public Entry getEntry(String key) {
        return mEntries.get(key);
    }

    /**
     * Replaces an existing entry with a new one.
     *
     * @param key The key of the {@link Entry} to replace.
     * @param updatedEntry The new {@link Entry} to place in place of the existing one.
     */
    public void updateEntry(String key, Entry updatedEntry) {
        mEntries.put(key, updatedEntry);
    }

    /**
     * Removes a preference entry from the index without disabling its target fragment.
     *
     * <p>This method should be used when a link to a fragment is being hidden from one screen, but
     * the fragment itself is still reachable via another path and should remain searchable.
     *
     * <p>For example, when the "Appearance" setting is enabled, the top-level link to "Tabs" on the
     * main settings screen is removed, but the "Tabs" screen is still accessible through the
     * "Appearance" screen. In this case, the MainSettings provider should call this method to
     * remove only the redundant link, without disabling the {@code TabsSettings} fragment.
     *
     * @param key The unique key of the preference link to remove.
     */
    public void removeEntry(String key) {
        mEntries.remove(key);
    }

    /** Set the flag indicating the index became stale and needs reindexing. */
    public void setNeedsIndexing() {
        // TODO(crbug.com/456817438): Implement this.
    }

    /** Return whether the index data needs to be refreshed. */
    public boolean needsIndexing() {
        // TODO(crbug.com/456817438): Implement this.
        return false;
    }

    /**
     * Clears all indexed entries and disabled fragments. This should be called before starting a
     * new indexing process.
     */
    public void clear() {
        mEntries.clear();
        mChildFragmentToParentKeys.clear();
    }

    /**
     * Registers a potential parent-child relationship between a preference and a fragment.
     *
     * @param childFragmentName The class name of the child fragment.
     * @param parentPreferenceKey The key of the preference that links to the child fragment.
     */
    public void addChildParentLink(String childFragmentName, String parentPreferenceKey) {
        mChildFragmentToParentKeys
                .computeIfAbsent(childFragmentName, k -> new ArrayList<>())
                .add(parentPreferenceKey);
    }

    /**
     * Finalizes the index by resolving the correct header for each entry based on the currently
     * visible preferences and removes any orphaned entries that no longer have a valid parent path.
     *
     * @param rootFragmentName The class name of the root fragment (e.g., MainSettings).
     */
    public void resolveIndex(String rootFragmentName) {
        Map<String, String> resolvedHeaderCache = new HashMap<>();
        List<String> entriesToRemove = new ArrayList<>();

        for (Entry entry : mEntries.values()) {
            // Root entries have their own title as the header if they do not inherit one from the
            // XML.
            if (entry.parentFragment.equals(rootFragmentName)) {
                if (TextUtils.isEmpty(entry.header)) {
                    Entry updatedEntry = new Entry.Builder(entry).setHeader(entry.title).build();
                    updateEntry(entry.id, updatedEntry);
                }
                continue;
            }

            String header =
                    findVisibleHeader(entry.parentFragment, resolvedHeaderCache, rootFragmentName);
            if (header != null) {
                Entry updatedEntry = new Entry.Builder(entry).setHeader(header).build();
                updateEntry(entry.id, updatedEntry);
            } else {
                // This entry is an orphan, we mark it for removal.
                entriesToRemove.add(entry.id);
            }
        }

        for (String key : entriesToRemove) {
            removeEntry(key);
        }
    }

    /**
     * Finds the header for a given fragment by searching upwards through its possible parents to
     * find a visible top-level setting.
     *
     * <p>This method uses a cache to avoid re-computing results for the same fragment and to
     * prevent infinite loops. A path is considered valid only if all its ancestor preferences still
     * exist in the index after the pruning phase.
     *
     * @param fragmentName The fragment to find the header for.
     * @param cache A map to store results of this traversal, preventing redundant work.
     * @param rootFragmentName The fragment that acts as the root of the settings hierarchy
     *     (MainSettings).
     * @return The title of the top-level preference that leads to this fragment, or {@code null} if
     *     no valid path back to the root can be found (i.e., the fragment is an orphan).
     */
    @Nullable
    private String findVisibleHeader(
            String fragmentName, Map<String, String> cache, String rootFragmentName) {
        if (cache.containsKey(fragmentName)) {
            return cache.get(fragmentName);
        }

        if (fragmentName.equals(rootFragmentName)) {
            return null; // The root has no parent header.
        }

        List<String> parentKeys = mChildFragmentToParentKeys.get(fragmentName);
        if (parentKeys == null) {
            cache.put(fragmentName, null); // No registered parent.
            return null;
        }

        // Find the valid parent preference that is still enabled. Given how our settings are
        // structured, a preference only has one valid parent at run-time. Otherwise, this would be
        // the first valid path.
        for (String parentKey : parentKeys) {
            Entry parentEntry = mEntries.get(parentKey);
            if (parentEntry != null) {
                // We have two scenarios here:
                // 1- This is a top-level preference under the main view. Then the title is the
                // header.
                // 2- This is not a top-level preference. We need to traverse up to find our
                // top-level preference that serves as an entry point for this pref.
                if (parentEntry.parentFragment.equals(rootFragmentName)) {
                    String header = parentEntry.title;
                    cache.put(fragmentName, header);
                    return header;
                } else {
                    String header =
                            findVisibleHeader(parentEntry.parentFragment, cache, rootFragmentName);
                    if (header != null) {
                        cache.put(fragmentName, header);
                        return header;
                    }
                }
            }
        }

        // We assign null here to track that we did query this and it resulted in no header. This
        // helps us avoid going up this path again.
        cache.put(fragmentName, null);
        return null;
    }

    /**
     * A container for the results of a search operation.
     *
     * <p>This class holds a list of {@link Entry} objects that matched a user's query. The items
     * are automatically sorted in descending order of relevance score upon being added.
     */
    public static class SearchResults {
        private final List<Map.Entry<Integer, Entry>> mScoredItems = new ArrayList<>();

        /** Return whether there was any search result. */
        public boolean isEmpty() {
            return mScoredItems.isEmpty();
        }

        /**
         * Add a matching entry to the result.
         *
         * @param item Matching entry to add.
         * @param score Matching score.
         */
        public void addItem(Entry item, int score) {
            // Add items in descending order of score.
            int i = 0;
            for (; i < mScoredItems.size(); ++i) {
                if (mScoredItems.get(i).getKey() < score) break;
            }
            mScoredItems.add(i, Map.entry(score, item));
        }

        /** Returns a list of search results. */
        public List<Entry> getItems() {
            List<Entry> entryList = new ArrayList<>();
            for (Map.Entry<Integer, Entry> pair : mScoredItems) {
                entryList.add(pair.getValue());
            }
            return entryList;
        }

        /** Returns a list of search results after grouping them by the header. */
        public List<Entry> groupByHeader() {
            Map<String, Integer> groups = new HashMap<>();
            List<Entry> results = new ArrayList<>();
            int pos = 0;

            // The input is already sorted by the score. Move up items till
            // they all get grouped together.
            for (Map.Entry<Integer, Entry> pair : mScoredItems) {
                Entry entry = pair.getValue();
                String header = entry.header;
                int groupPos = groups.getOrDefault(header, -1);
                if (groupPos < 0) {
                    // |groups| keep the position of the lowest entries of each group.
                    // The new item with the same group is inserted in that position.
                    groups.put(header, pos);
                    results.add(entry);
                } else {
                    // Push down all the items not in |header| and add the new one there.
                    if (groupPos == results.size() - 1) {
                        results.add(entry);
                    } else {
                        results.add(groupPos + 1, entry);
                    }
                    Map<String, Integer> newGroups = new HashMap<>();
                    for (String key : groups.keySet()) {
                        // Adjust |groups| after a new item is inserted. Any group
                        // below the current pos should be pushed down by one.
                        int p = groups.get(key);
                        newGroups.put(key, groupPos <= p ? p + 1 : p);
                    }
                    groups = newGroups;
                }
                ++pos;
            }
            return results;
        }
    }

    /**
     * Performs an in-memory search for the given query against all indexed preferences.
     *
     * <p>The search logic is as follows:
     *
     * <ol>
     *   <li>The user's query is normalized (converted to lowercase and diacritics are stripped).
     *   <li>The method iterates through all indexed {@link Entry} objects.
     *   <li>It performs a case-insensitive, diacritic-insensitive search against the normalized
     *       title and summary of each entry.
     *   <li>A scoring model is applied to rank results: an exact title match receives the highest
     *       score, followed by a partial title match, followed by a summary match.
     *   <li>An entry is added to the results list at most once, with the score of its best matching
     *       field.
     * </ol>
     *
     * @param query The user's search query. Can be null or empty.
     * @return A {@link SearchResults} object containing the list of matching {@link Entry} objects,
     *     sorted by relevance score. Returns an empty result for null or empty queries.
     */
    public SearchResults search(String query) {
        query = normalizeString(query);
        SearchResults results = new SearchResults();

        if (TextUtils.isEmpty(query)) {
            return results;
        }

        for (Entry entry : mEntries.values()) {
            if (entry.mTitleNormalized != null && entry.mTitleNormalized.contains(query)) {
                int score =
                        TextUtils.equals(entry.mTitleNormalized, query)
                                ? EXACT_TITLE_MATCH
                                : PARTIAL_TITLE_MATCH;
                results.addItem(entry, score);
                continue;
            }

            if (entry.mSummaryNormalized != null && entry.mSummaryNormalized.contains(query)) {
                results.addItem(entry, PARTIAL_SUMMARY_MATCH);
            }
        }
        return results;
    }

    Map<String, Entry> getEntriesForTesting() {
        return mEntries;
    }

    Map<String, List<String>> getChildFragmentToParentKeysForTesting() {
        return mChildFragmentToParentKeys;
    }
}
