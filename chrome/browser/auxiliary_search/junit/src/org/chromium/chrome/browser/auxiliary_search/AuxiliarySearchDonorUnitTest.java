// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.annotation.NonNull;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.SearchResult;
import androidx.appsearch.app.SearchResults;
import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.app.SetSchemaResponse.MigrationFailure;
import androidx.appsearch.builtintypes.GlobalSearchApplicationInfo;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;

import com.google.common.util.concurrent.ListenableFuture;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchDonor.SearchQueryChecker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for AuxiliarySearchDonor. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mock ListenableFuture.
public class AuxiliarySearchDonorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private static final int DEFAULT_TAB_TTL_HOURS = 168;
    private static final int DEFAULT_HISTORY_TTL_HOURS = 24;

    @Mock private MigrationFailure mMigrationFailure;
    @Mock private AuxiliarySearchHooks mHooks;
    @Mock private ListenableFuture<AppSearchSession> mAppSearchSession;
    @Mock private AppSearchSession mSession;
    @Mock private Callback<Boolean> mCallback;

    private AuxiliarySearchDonor mAuxiliarySearchDonor;

    @Before
    public void setUp() {
        when(mHooks.isEnabled()).thenReturn(true);
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(mHooks);
        assertTrue(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        AuxiliarySearchDonor.setSkipInitializationForTesting(true);
        createAndInitAuxiliarySearchDonor();
    }

    @After
    public void tearDown() {
        mFakeTime.resetTimes();
    }

    @Test
    public void testCreateSessionAndInit() {
        // #createSessionAndInit() has been called in AuxiliarySearchDonor's constructor.
        // Verifies that calling createSessionAndInit() again will early exit.
        assertFalse(mAuxiliarySearchDonor.createSessionAndInit());
        assertTrue(mAuxiliarySearchDonor.getIsCreatedSessionAndInitForTesting());
    }

    @Test
    public void testCreateSessionAndInit_DefaultDisabled() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());

        mAuxiliarySearchDonor = AuxiliarySearchDonor.createDonorForTesting();
        assertTrue(mAuxiliarySearchDonor.getIsCreatedSessionAndInitForTesting());
    }

    @Test
    public void testDefaultTtlIsNotZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getTabDocumentTtlMs());
        assertEquals(
                DEFAULT_TAB_TTL_HOURS * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getTabDocumentTtlMs());

        assertEquals(
                DEFAULT_HISTORY_TTL_HOURS * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getHistoryDocumentTtlMs());
    }

    @Test
    @EnableFeatures("AndroidAppIntegration:content_ttl_hours/0")
    public void testConfiguredTtlCannotBeZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getTabDocumentTtlMs());
        assertEquals(
                DEFAULT_TAB_TTL_HOURS * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getTabDocumentTtlMs());
    }

    @Test
    public void testCalculateDocumentTtlMs() {
        long creationTime = 10;
        long currentTime = 100;
        long expectedTtl = currentTime + DEFAULT_TAB_TTL_HOURS * 60 * 60 * 1000 - creationTime;
        assertEquals(
                expectedTtl,
                mAuxiliarySearchDonor.calculateDocumentTtlMs(
                        /* isTab= */ true, creationTime, currentTime));

        creationTime = 1743083874549L;
        expectedTtl = currentTime + DEFAULT_TAB_TTL_HOURS * 60 * 60 * 1000 - creationTime;
        assertEquals(
                expectedTtl,
                mAuxiliarySearchDonor.calculateDocumentTtlMs(
                        /* isTab= */ true, creationTime, currentTime));

        expectedTtl = currentTime + DEFAULT_HISTORY_TTL_HOURS * 60 * 60 * 1000 - creationTime;
        assertEquals(
                expectedTtl,
                mAuxiliarySearchDonor.calculateDocumentTtlMs(
                        /* isTab= */ false, creationTime, currentTime));
    }

    @Test
    public void testBuildDocument_Tab() {
        int id = 10;
        int type = AuxiliarySearchEntryType.TAB;
        GURL url = JUnitTestGURLs.URL_1;
        String title = "Title";
        long lastAccessTimeStamp = 100;
        long currentTime = TimeUtils.currentTimeMillis();
        long documentTtl =
                currentTime + TimeUnit.HOURS.toMillis(DEFAULT_TAB_TTL_HOURS) - lastAccessTimeStamp;
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];

        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(url);
        when(tab.getTitle()).thenReturn(title);
        when(tab.getTimestampMillis()).thenReturn(lastAccessTimeStamp);
        when(tab.getId()).thenReturn(id);

        testBuildDocumentImplAndVerify(
                tab,
                type,
                url.getSpec(),
                title,
                lastAccessTimeStamp,
                id,
                documentTtl,
                /* score= */ 0,
                AuxiliarySearchDonor.SOURCE_TAB,
                counts,
                currentTime);
        assertEquals(1, counts[type]);
    }

    @Test
    public void testBuildDocument_AuxiliarySearchEntry() {
        int id = 10;
        int type = AuxiliarySearchEntryType.TAB;
        String url = "Url";
        String title = "Title";
        long lastAccessTimeStamp = 100;
        long currentTime = TimeUtils.currentTimeMillis();

        long documentTtl =
                currentTime + TimeUnit.HOURS.toMillis(DEFAULT_TAB_TTL_HOURS) - lastAccessTimeStamp;
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];

        var builder =
                AuxiliarySearchEntry.newBuilder()
                        .setTitle(title)
                        .setUrl(url)
                        .setId(id)
                        .setLastAccessTimestamp(lastAccessTimeStamp);
        AuxiliarySearchEntry entry = builder.build();

        testBuildDocumentImplAndVerify(
                entry,
                type,
                url,
                title,
                lastAccessTimeStamp,
                id,
                documentTtl,
                /* score= */ 0,
                AuxiliarySearchDonor.SOURCE_TAB,
                counts,
                currentTime);
        assertEquals(1, counts[type]);
    }

    @Test
    public void testBuildDocument_AuxiliarySearchDataEntry() {
        int id = 10;
        GURL url = JUnitTestGURLs.URL_1;
        String title = "Title";
        long lastAccessTimeStamp = 100;
        long currentTime = TimeUtils.currentTimeMillis();
        long tabDocumentTtl =
                currentTime + TimeUnit.HOURS.toMillis(DEFAULT_TAB_TTL_HOURS) - lastAccessTimeStamp;
        long historyDocumentTtl =
                currentTime
                        + TimeUnit.HOURS.toMillis(DEFAULT_HISTORY_TTL_HOURS)
                        - lastAccessTimeStamp;
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];

        int type = AuxiliarySearchEntryType.TAB;
        AuxiliarySearchDataEntry entry =
                new AuxiliarySearchDataEntry(
                        type,
                        url,
                        title,
                        lastAccessTimeStamp,
                        id,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0);

        int visitId = 100;
        int type2 = AuxiliarySearchEntryType.CUSTOM_TAB;
        AuxiliarySearchDataEntry entry2 =
                new AuxiliarySearchDataEntry(
                        type2,
                        url,
                        title,
                        lastAccessTimeStamp,
                        Tab.INVALID_TAB_ID,
                        /* appId= */ null,
                        visitId,
                        /* score= */ 0);

        int visitId3 = 101;
        int type3 = AuxiliarySearchEntryType.TOP_SITE;
        AuxiliarySearchDataEntry entry3 =
                new AuxiliarySearchDataEntry(
                        type3,
                        url,
                        title,
                        lastAccessTimeStamp,
                        Tab.INVALID_TAB_ID,
                        /* appId= */ null,
                        visitId3,
                        AuxiliarySearchTestHelper.SCORE_1);

        testBuildDocumentImplAndVerify(
                entry,
                type,
                url.getSpec(),
                title,
                lastAccessTimeStamp,
                id,
                tabDocumentTtl,
                /* score= */ 0,
                AuxiliarySearchDonor.SOURCE_TAB,
                counts,
                currentTime);
        testBuildDocumentImplAndVerify(
                entry2,
                type2,
                url.getSpec(),
                title,
                lastAccessTimeStamp,
                visitId,
                historyDocumentTtl,
                /* score= */ 0,
                AuxiliarySearchDonor.SOURCE_CUSTOM_TAB,
                counts,
                currentTime);
        testBuildDocumentImplAndVerify(
                entry3,
                type3,
                url.getSpec(),
                title,
                lastAccessTimeStamp,
                visitId3,
                historyDocumentTtl,
                AuxiliarySearchTestHelper.SCORE_1,
                AuxiliarySearchDonor.SOURCE_TOP_SITE,
                counts,
                currentTime);
        assertEquals(1, counts[type]);
        assertEquals(1, counts[type2]);
        assertEquals(1, counts[type3]);
    }

    private <T> void testBuildDocumentImplAndVerify(
            T entry,
            @AuxiliarySearchEntryType int type,
            String url,
            String title,
            long lastAccessTimeStamp,
            int id,
            long documentTtlMs,
            int score,
            String source,
            int[] counts,
            long currentTime) {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.RGB_565);
        String documentId = AuxiliarySearchDonor.getDocumentId(type, id);

        WebPage webPage = mAuxiliarySearchDonor.buildDocument(entry, bitmap, counts, currentTime);

        assertEquals(documentId, webPage.getId());
        assertEquals(url, webPage.getUrl());
        assertEquals(title, webPage.getName());
        assertEquals(lastAccessTimeStamp, webPage.getCreationTimestampMillis());
        assertEquals(documentTtlMs, webPage.getDocumentTtlMillis());
        assertEquals(score, webPage.getDocumentScore());
        assertEquals(source, webPage.getSource());
        assertTrue(
                Arrays.equals(
                        AuxiliarySearchUtils.bitmapToBytes(bitmap),
                        webPage.getFavicon().getBytes()));
    }

    @Test
    public void testOnSetSchemaResponseAvailable() {
        List<MigrationFailure> migrationFailures = new ArrayList<>();
        migrationFailures.add(mMigrationFailure);
        SetSchemaResponse setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        List<WebPage> pendingDocs = new ArrayList<>();
        WebPage webPage = new WebPage.Builder("namespace", "Id1").setUrl("Url1").build();
        pendingDocs.add(webPage);
        mAuxiliarySearchDonor.setPendingDocumentsForTesting(pendingDocs);
        mAuxiliarySearchDonor.resetSchemaSetForTesting();

        // Verifies that the pending donation isn't executed if setSchema is failed.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNotNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        migrationFailures.clear();
        setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        // Verifies that the pending donation will be executed if setSchema succeeds.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertEquals(
                AuxiliarySearchUtils.CURRENT_SCHEMA_VERSION,
                AuxiliarySearchUtils.getSchemaVersion());
    }

    @Test
    public void testGetDocumentId() {
        int id = 10;
        String tabDocumentId = "Tab-10";
        String customTabDocumentId = "CustomTab-10";
        String topSiteDocumentId = "TopSite-10";

        assertEquals(
                tabDocumentId,
                AuxiliarySearchDonor.getDocumentId(AuxiliarySearchEntryType.TAB, id));
        assertEquals(
                customTabDocumentId,
                AuxiliarySearchDonor.getDocumentId(AuxiliarySearchEntryType.CUSTOM_TAB, id));
        assertEquals(
                topSiteDocumentId,
                AuxiliarySearchDonor.getDocumentId(AuxiliarySearchEntryType.TOP_SITE, id));
    }

    @Test
    public void testSharedPreferenceKeyIsUpdated() {
        SetSchemaResponse setSchemaResponse = new SetSchemaResponse.Builder().build();
        assertTrue(setSchemaResponse.getMigrationFailures().isEmpty());

        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertFalse(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        // Verifies that ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET is set to true after
        // the schema is set successful.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertTrue(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    public void testDoNotSetSchemaAgain() {
        mAuxiliarySearchDonor.resetSchemaSetForTesting();
        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        chromeSharedPreferences.writeBoolean(
                ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        AuxiliarySearchUtils.setSchemaVersion(AuxiliarySearchUtils.CURRENT_SCHEMA_VERSION);
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        // Verifies that #onConsumerSchemaSearchedImpl() returns false, i.e., not to set the schema
        // again if it has been set.
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    public void testOnConfigChanged() {
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(false, callback);
        assertFalse(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(true, callback);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());
    }

    @Test
    public void testCanDonate() {
        mAuxiliarySearchDonor.setSharedTabsWithOsStateForTesting(
                /* sharedTabsWithOsState= */ false);
        assertFalse(mAuxiliarySearchDonor.canDonate());

        mAuxiliarySearchDonor.setSharedTabsWithOsStateForTesting(/* sharedTabsWithOsState= */ true);
        mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ false);
        assertFalse(mAuxiliarySearchDonor.canDonate());

        mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true);
        assertTrue(mAuxiliarySearchDonor.canDonate());
    }

    @Test
    public void testOnConsumerSchemaSearchedImpl() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        AuxiliarySearchUtils.setSchemaVersion(AuxiliarySearchUtils.CURRENT_SCHEMA_VERSION);
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        mAuxiliarySearchDonor.setPendingCallbackForTesting(callback);

        // Verifies that closeSession() which calls the pending callback is executed when device
        // isn't capable for Tabs donation while previously donation was allowed (schema is set).
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ false));
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));
        verify(callback).onResult(eq(false));

        // Verifies that onConsumerSchemaSearchedImpl() doesn't reset the schema thus returns
        // false, while AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND is set to be true.
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));

        // Verifies that onConsumerSchemaSearchedImpl() returns true to set the schema, and
        // AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND is set to be true.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false);
        assertTrue(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));

        // Verifies that onConsumerSchemaSearchedImpl() returns true to set the schema if the
        // AUXILIARY_SEARCH_SCHEMA_VERSION doesn't match the CURRENT_SCHEMA_VERSION.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_SCHEMA_VERSION);
        assertEquals(0, AuxiliarySearchUtils.getSchemaVersion());
        assertTrue(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));
    }

    @Test
    public void testIterateSearchResults() {
        SearchResults searchresults = Mockito.mock(SearchResults.class);
        SearchQueryChecker searchQueryChecker = Mockito.mock(SearchQueryChecker.class);
        List<SearchResult> page = new ArrayList<>();
        assertTrue(page.isEmpty());

        mAuxiliarySearchDonor.iterateSearchResults(
                searchresults, page, mCallback, searchQueryChecker);
        verify(mCallback).onResult(eq(false));

        SearchResult searchResult1 =
                createSearchResult(
                        GlobalSearchApplicationInfo.APPLICATION_TYPE_PRODUCER,
                        AuxiliarySearchDonor.SCHEMA_WEBPAGE);
        SearchResult searchResult2 =
                createSearchResult(GlobalSearchApplicationInfo.APPLICATION_TYPE_CONSUMER, "Schema");
        SearchResult searchResult3 =
                createSearchResult(
                        GlobalSearchApplicationInfo.APPLICATION_TYPE_CONSUMER,
                        AuxiliarySearchDonor.SCHEMA_WEBPAGE);

        when(searchQueryChecker.isSuccess(eq(searchResult1))).thenReturn(false);
        page.add(searchResult1);
        mAuxiliarySearchDonor.iterateSearchResults(
                searchresults, page, mCallback, searchQueryChecker);
        verify(mCallback, times(2)).onResult(eq(false));

        when(searchQueryChecker.isSuccess(eq(searchResult2))).thenReturn(false);
        page.add(searchResult2);
        mAuxiliarySearchDonor.iterateSearchResults(
                searchresults, page, mCallback, searchQueryChecker);
        verify(mCallback, times(3)).onResult(eq(false));

        when(searchQueryChecker.isSuccess(eq(searchResult3))).thenReturn(true);
        page.add(searchResult3);
        mAuxiliarySearchDonor.iterateSearchResults(
                searchresults, page, mCallback, searchQueryChecker);
        verify(mCallback).onResult(eq(true));
    }

    @Test
    public void testIsShareTabsWithOsEnabledKeyExist() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.SHARING_TABS_WITH_OS);

        assertFalse(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());

        prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true);
        assertTrue(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());

        prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, false);
        assertTrue(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());
    }

    private SearchResult createSearchResult(int applicationType, @NonNull String schemaType) {
        GlobalSearchApplicationInfo appInfo =
                new GlobalSearchApplicationInfo.Builder("namespace", "id", applicationType)
                        .setSchemaTypes(Arrays.asList(schemaType))
                        .build();
        try {
            return new SearchResult.Builder("package", "database").setDocument(appInfo).build();
        } catch (AppSearchException e) {
            return null;
        }
    }

    private void createAndInitAuxiliarySearchDonor() {
        mAuxiliarySearchDonor = AuxiliarySearchDonor.createDonorForTesting();
        try {
            when(mAppSearchSession.get()).thenReturn(mSession);
            mAuxiliarySearchDonor.setAppSearchSessionForTesting(mAppSearchSession);
        } catch (Exception e) {
            // Just continue.
        }
    }
}
