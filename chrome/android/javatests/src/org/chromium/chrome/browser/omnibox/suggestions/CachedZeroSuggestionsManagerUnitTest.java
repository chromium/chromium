// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.ArraySet;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.AutocompleteResult.GroupDetails;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link CachedZeroSuggestionsManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CachedZeroSuggestionsManagerUnitTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        final List<AutocompleteMatch> list1 = data1.getSuggestionsList();
        final List<AutocompleteMatch> list2 = data2.getSuggestionsList();
        Assert.assertEquals(list1, list2);

        final SparseArray<GroupDetails> groupsDetails1 = data1.getGroupsDetails();
        final SparseArray<GroupDetails> groupsDetails2 = data2.getGroupsDetails();
        Assert.assertEquals(groupsDetails1.size(), groupsDetails2.size());
        for (int index = 0; index < groupsDetails1.size(); index++) {
            Assert.assertEquals(groupsDetails1.keyAt(index), groupsDetails2.keyAt(index));
            Assert.assertEquals(groupsDetails1.valueAt(index), groupsDetails2.valueAt(index));
        }
    }

    /**
     * Build a dummy suggestions list.
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<AutocompleteMatch> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            final int id = index + 1;
            list.add(createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL)
                             .setPostContentType(hasPostData ? "Content Type " + id : null)
                             .setPostData(hasPostData ? new byte[] {4, 5, 6, (byte) id} : null)
                             .build());
        }

        return list;
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @return Newly constructed AutocompleteMatch.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(int id) {
        return createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL);
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @param type Suggestion type.
     * @return Newly constructed AutocompleteMatch.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(
            int id, @OmniboxSuggestionType int type) {
        return AutocompleteMatchBuilder.searchWithType(type)
                .setDisplayText("dummy text " + id)
                .setDescription("dummy description " + id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void setNewSuggestions_cachedSuggestionsWithPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(2, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void setNewSuggestions_cachedSuggestionsWithoutPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(2, false), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void setNewSuggestions_DoNotcacheClipboardSuggestions() {
        List<AutocompleteMatch> mix_list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.CLIPBOARD_IMAGE).build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                createSuggestionBuilder(3, OmniboxSuggestionType.CLIPBOARD_TEXT).build(),
                createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());
        List<AutocompleteMatch> expected_list =
                Arrays.asList(createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                        createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());

        AutocompleteResult dataToCache = new AutocompleteResult(mix_list, null);
        AutocompleteResult dataToExpected = new AutocompleteResult(expected_list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToExpected, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void groupsDetails_restoreDetailsFromEmptyCache() {
        // Note: purge cache explicitly, because tests are run on an actual device
        // and cache may hold content from other test runs.
        AutocompleteResult dataToCache = new AutocompleteResult(null, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void groupsDetails_cacheAllSaneGroupDetails() {
        SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(10, new GroupDetails("Header For Group 10", false));
        groupsDetails.put(20, new GroupDetails("Header For Group 20", false));
        groupsDetails.put(30, new GroupDetails("Header For Group 30", false));
        AutocompleteResult dataToCache = new AutocompleteResult(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void groupsDetails_cachePartiallySaneGroupDetailsDropsInvalidEntries() {
        SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(10, new GroupDetails("Header For Group 10", false));
        groupsDetails.put(
                AutocompleteMatch.INVALID_GROUP, new GroupDetails("Header For Group 20", true));
        groupsDetails.put(30, new GroupDetails("", false));

        AutocompleteResult dataToCache = new AutocompleteResult(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();

        SparseArray<GroupDetails> validGroupsDetails = new SparseArray<>();
        validGroupsDetails.put(10, new GroupDetails("Header For Group 10", false));
        assertAutocompleteResultEquals(
                dataFromCache, new AutocompleteResult(null, validGroupsDetails));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void groupsDetails_restoreInvalidGroupsDetailsFromCache() {
        SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(
                AutocompleteMatch.INVALID_GROUP, new GroupDetails("This group is invalid", true));
        groupsDetails.put(20, new GroupDetails("", false));
        groupsDetails.put(30, new GroupDetails("", false));

        AutocompleteResult dataToCache = new AutocompleteResult(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, new AutocompleteResult(null, null));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void dropSuggestions_suggestionsWithValidGroupsAssociation() {
        List<AutocompleteMatch> list = buildDummySuggestionsList(2, false);
        list.add(createSuggestionBuilder(33).setGroupId(1).build());
        SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(1, new GroupDetails("Valid Header", true));

        AutocompleteResult dataToCache = new AutocompleteResult(list, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void dropSuggestions_suggestionsWithInvalidGroupsAssociation() {
        List<AutocompleteMatch> listExpected = buildDummySuggestionsList(2, false);
        List<AutocompleteMatch> listToCache = buildDummySuggestionsList(2, false);
        listToCache.add(createSuggestionBuilder(33).setGroupId(1).build());

        AutocompleteResult dataExpected = new AutocompleteResult(listExpected, null);
        AutocompleteResult dataToCache = new AutocompleteResult(listToCache, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataExpected, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void malformedCache_dropsMissingSuggestions() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Save one valid suggestion to cache.
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(1, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Signal that there's actually 2 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 2);

        // Construct an expected raw suggestion list content. This constitutes one valid entry
        // and 1 totally empty entry.
        AutocompleteResult rawDataFromCache =
                new AutocompleteResult(buildDummySuggestionsList(1, true), null);
        rawDataFromCache.getSuggestionsList().add(new AutocompleteMatchBuilder().build());

        // readCachedSuggestionList makes full attempt to restore whatever could be scraped from the
        // cache.
        List<AutocompleteMatch> readList =
                CachedZeroSuggestionsManager.readCachedSuggestionList(manager);
        Assert.assertEquals(2, readList.size());
        assertAutocompleteResultEquals(new AutocompleteResult(readList, null), rawDataFromCache);

        // Cache recovery however should be smart here and remove items that make no sense.
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, dataToCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void malformedCache_dropsMissingGroupDetails() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Write 3 wrong group groupsDetails to the cache
        SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(12, new GroupDetails("Valid group", true));
        groupsDetails.put(34, new GroupDetails("", false));
        groupsDetails.put(AutocompleteMatch.INVALID_GROUP, new GroupDetails("Invalid group", true));
        AutocompleteResult invalidDataToCache = new AutocompleteResult(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(invalidDataToCache);

        // Report that we actually have 4 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, 4);

        // Read raw suggestions from the cache. Note that the sparse array will only have 3 elements
        // because we put one item with INVALID_GROUP, and additional INVALID_GROUP will be deduced
        // from missing data with null title and default expanded state set to true.
        SparseArray<GroupDetails> rawGroupsDetails =
                CachedZeroSuggestionsManager.readCachedGroupsDetails(manager);
        Assert.assertEquals(3, rawGroupsDetails.size());
        Assert.assertEquals(rawGroupsDetails.get(12).title, "Valid group");
        Assert.assertEquals(rawGroupsDetails.get(12).collapsedByDefault, true);
        Assert.assertEquals(rawGroupsDetails.get(34).title, "");
        Assert.assertEquals(rawGroupsDetails.get(34).collapsedByDefault, false);
        Assert.assertEquals(rawGroupsDetails.get(AutocompleteMatch.INVALID_GROUP).title, null);
        Assert.assertEquals(
                rawGroupsDetails.get(AutocompleteMatch.INVALID_GROUP).collapsedByDefault, false);

        // Cache recovery however should be smart here and remove items that make no sense.
        SparseArray<GroupDetails> wantGroupsDetails = new SparseArray<>();
        wantGroupsDetails.put(12, new GroupDetails("Valid group", true));
        AutocompleteResult wantDataFromCache = new AutocompleteResult(null, wantGroupsDetails);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();

        assertAutocompleteResultEquals(dataFromCache, wantDataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void removeInvalidSuggestions_dropsInvalidSuggestionsAndGroupsDetails() {
        // Write 3 wrong group groupsDetails to the cache
        SparseArray<GroupDetails> groupsDetailsExpected = new SparseArray<>();
        groupsDetailsExpected.put(12, new GroupDetails("Valid group", true));

        SparseArray<GroupDetails> groupsDetailsWithInvalidItems = new SparseArray<>();
        groupsDetailsWithInvalidItems.put(12, new GroupDetails("Valid group", true));
        groupsDetailsWithInvalidItems.put(34, new GroupDetails("", false));
        groupsDetailsWithInvalidItems.put(
                AutocompleteMatch.INVALID_GROUP, new GroupDetails("Invalid group", true));

        List<AutocompleteMatch> listExpected = buildDummySuggestionsList(2, false);
        listExpected.add(createSuggestionBuilder(72).setGroupId(12).build());

        List<AutocompleteMatch> listWithInvalidItems = buildDummySuggestionsList(2, false);
        listWithInvalidItems.add(createSuggestionBuilder(72).setGroupId(12).build());
        listWithInvalidItems.add(
                createSuggestionBuilder(73).setGroupId(12).setUrl(new GURL("bad URL")).build());
        listWithInvalidItems.add(createSuggestionBuilder(74).setGroupId(34).build());

        AutocompleteResult dataWithInvalidItems =
                new AutocompleteResult(listWithInvalidItems, groupsDetailsWithInvalidItems);
        AutocompleteResult dataExpected =
                new AutocompleteResult(listExpected, groupsDetailsExpected);

        CachedZeroSuggestionsManager.removeInvalidSuggestionsAndGroupsDetails(
                dataWithInvalidItems.getSuggestionsList(), dataWithInvalidItems.getGroupsDetails());
        assertAutocompleteResultEquals(dataWithInvalidItems, dataExpected);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void cacheAndRestoreSuggestionSubtypes() {
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                        .addSubtype(1)
                        .addSubtype(4)
                        .build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL)
                        .addSubtype(17)
                        .build(),
                createSuggestionBuilder(3, OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                        .addSubtype(2)
                        .addSubtype(10)
                        .addSubtype(30)
                        .build(),
                createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());

        AutocompleteResult dataToCache = new AutocompleteResult(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void rejectCacheIfSubtypesAreMalformed() {
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                        .addSubtype(1)
                        .addSubtype(4)
                        .build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL)
                        .addSubtype(17)
                        .build());

        AutocompleteResult dataToCache = new AutocompleteResult(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Insert garbage for the Suggestion Subtypes.
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("invalid");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(1),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(new AutocompleteResult(null, null), dataFromCache);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void rejectCacheIfSubtypesIncludeNull() {
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                        .addSubtype(1)
                        .build());

        AutocompleteResult dataToCache = new AutocompleteResult(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("null");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(0),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(new AutocompleteResult(null, null), dataFromCache);
    }
}
