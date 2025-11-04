// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.text.Normalizer;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Data from Preferences used for settings search. This is a collection of data to be indexed. */
@NullMarked
public class SettingsIndexData {

    /* Scores given to the entries having matches with a query */
    public static final int EXACT_TITLE_MATCH = 10;
    public static final int PARTIAL_TITLE_MATCH = 5;
    public static final int PARTIAL_SUMMARY_MATCH = 3;

    private final Map<String, Entry> mEntries = new LinkedHashMap<>();
    private final Set<String> mDisabledFragments = new HashSet<>();

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
        /** Key defined for Preference/Fragment. */
        public final String key;

        /** Title of Preference/Fragment. */
        public final String title;

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

        private final @Nullable String mTitleNormalized;
        private final @Nullable String mSummaryNormalized;

        private Entry(
                String key,
                String title,
                @Nullable String header,
                @Nullable String summary,
                @Nullable String fragment,
                String parentFragment,
                @Nullable String titleNormalized,
                @Nullable String summaryNormalized) {
            this.key = key;
            this.title = title;
            this.header = header;
            this.summary = summary;
            this.fragment = fragment;
            this.parentFragment = parentFragment;
            mTitleNormalized = titleNormalized;
            mSummaryNormalized = summaryNormalized;
        }

        /**
         * A builder for creating immutable {@link Entry} objects. For future modifications, please
         * be aware of the normalized fields.
         */
        public static class Builder {
            private final String mKey;
            private final String mTitle;
            private @Nullable String mHeader;
            private @Nullable String mSummary;
            private @Nullable String mFragment;
            private final String mParentFragment;

            /**
             * Constructs a builder with the minimum required fields for creating a new {@link
             * Entry}.
             *
             * @param key The unique key of the preference.
             * @param title The title of the preference.
             * @param parentFragment The class name of the fragment containing this preference.
             */
            public Builder(String key, String title, String parentFragment) {
                mKey = key;
                mTitle = title;
                mParentFragment = parentFragment;
            }

            /**
             * Constructs a builder by copying the state from an existing {@link Entry}.
             *
             * @param original The original {@link Entry} to copy.
             */
            public Builder(Entry original) {
                mKey = original.key;
                mTitle = original.title;
                mHeader = original.header;
                mSummary = original.summary;
                mFragment = original.fragment;
                mParentFragment = original.parentFragment;
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

            /**
             * Creates an {@link Entry} object.
             *
             * @return A new, immutable {@link Entry} instance.
             */
            public Entry build() {
                String titleNormalized = normalizeString(mTitle);
                String summaryNormalized = normalizeString(mSummary);

                return new Entry(
                        mKey,
                        mTitle,
                        mHeader,
                        mSummary,
                        mFragment,
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
     * Removes a preference entry from the index and performs hierarchical disabling.
     *
     * <p>If the removed entry is a fragment (i.e., its {@code fragment} field is not null), then it
     * is added to the "disabled" list, to be able to disable all the children settings as well.
     *
     * @param key The unique key of the preference to remove.
     */
    public void removeEntry(String key) {
        var entry = mEntries.get(key);
        if (entry == null) return;

        if (entry.fragment != null) {
            setDisabledFragment(entry.fragment);
        }
        mEntries.remove(key);
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
    public void removeSimpleEntry(String key) {
        mEntries.remove(key);
    }

    /**
     * Mark a given {@link Fragment} as disabled. This makes it possible to disable all the children
     * settings as well.
     *
     * @param fragment Full package name of a {@link Fragment}.
     */
    public void setDisabledFragment(String fragment) {
        mDisabledFragments.add(fragment);
    }

    /**
     * Whether a given {@link Fragment} is disabled.
     *
     * @param fragment Full package name of a {@link Fragment}.
     */
    public boolean isDisabledFragment(String fragment) {
        return mDisabledFragments.contains(fragment);
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
     * Initializes the in-memory search index for all settings. It uses the providers found in
     * {@link SearchIndexProviderRegistry.ALL_PROVIDERS}.
     */
    public void initIndex(Context context) {
        // This is done to avoid duplicate entries when parsing XML.
        mEntries.clear();
        mDisabledFragments.clear();

        for (SearchIndexProvider provider : SearchIndexProviderRegistry.ALL_PROVIDERS) {
            // This handles both the default "dumb" parsers and our custom "smart" override in
            // MainSettings.
            provider.initPreferenceXml(context, this);
            // This handles dynamic text updates (e.g., TabArchiveSettings) and code-only entries
            // (e.g., AboutChromeSettings).
            provider.updateDynamicPreferences(context, this);
        }
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
            assumeNonNull(entry.mTitleNormalized);
            if (entry.mTitleNormalized.contains(query)) {
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
}
