// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationV2ContentTtlHours;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.Signature;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;
import androidx.appsearch.app.AppSearchBatchResult;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.GenericDocument;
import androidx.appsearch.app.GlobalSearchSession;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SearchResult;
import androidx.appsearch.app.SearchResults;
import androidx.appsearch.app.SearchSpec;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.builtintypes.GlobalSearchApplicationInfo;
import androidx.appsearch.builtintypes.ImageObject;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;
import androidx.appsearch.platformstorage.PlatformStorage;

import com.google.common.util.concurrent.AsyncFunction;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/** This class handles the donation of Tabs. */
@NullMarked
public class AuxiliarySearchDonor {

    /** Callback to set schema visibilities for package names. */
    interface SetDocumentClassVisibilityForPackageCallback {
        /**
         * @param schemaClass The class type of the schema to set visibility.
         * @param packageName The package name of the app which can see the schema.
         * @param sha256Certificate The sha256 signing key of the app.
         */
        void setDocumentClassVisibility(
                Class<?> schemaClass, String packageName, String sha256Certificate);
    }

    /** A helper interface for iterating search results from the App Search. */
    interface SearchQueryChecker {
        /**
         * Returns whether it is the sought result.
         *
         * @param searchResult The current search result page.
         */
        boolean isSuccess(SearchResult searchResult);
    }

    @VisibleForTesting static final String SCHEMA = "builtin:GlobalSearchApplicationInfo";
    @VisibleForTesting static final String SCHEMA_WEBPAGE = "builtin:WebPage";
    @VisibleForTesting static final String SOURCE_TAB = "Tab";
    @VisibleForTesting static final String SOURCE_CUSTOM_TAB = "CustomTab";
    @VisibleForTesting static final String SOURCE_TOP_SITE = "TopSite";

    private static final String TAG = "AuxiliarySearchDonor";
    private static final String TAB_PREFIX = "Tab-";
    private static final String CUSTOM_TAB_PREFIX = "CustomTab-";
    private static final String TOP_SITE_PREFIX = "TopSite-";
    private static final Executor UI_THREAD_EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.UI_DEFAULT, r);
    private static boolean sSkipInitializationForTesting;

    private final Context mContext;
    private final String mNamespace;
    private final boolean mSkipSchemaCheck;

    private @Nullable ListenableFuture<AppSearchSession> mAppSearchSession;
    private @Nullable ListenableFuture<GlobalSearchSession> mGlobalSearchSession;
    private @Nullable Long mTabTtlMillis;
    private @Nullable Long mHistoryTtlMillis;
    private boolean mIsSchemaSet;
    private @Nullable List<WebPage> mPendingDocuments;
    private @Nullable Callback<Boolean> mPendingCallback;
    private boolean mSharedTabsWithOsState;
    private @Nullable Boolean mIsDeviceCompatible;
    private boolean mIsCreatedSessionAndInitForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final AuxiliarySearchDonor sInstance = new AuxiliarySearchDonor();
    }

    /** Returns the singleton instance of AuxiliarySearchDonor. */
    public static AuxiliarySearchDonor getInstance() {
        return AuxiliarySearchDonor.LazyHolder.sInstance;
    }

    private AuxiliarySearchDonor() {
        mContext = ContextUtils.getApplicationContext();
        mNamespace = mContext.getPackageName();
        mSkipSchemaCheck =
                AuxiliarySearchUtils.SKIP_SCHEMA_CHECK.getValue()
                        || AuxiliarySearchUtils.MULTI_DATA_SOURCE_SKIP_SCHEMA_CHECK.getValue();

        mSharedTabsWithOsState = AuxiliarySearchUtils.isShareTabsWithOsEnabled();
        boolean shouldInit = mSharedTabsWithOsState || !isShareTabsWithOsEnabledKeyExist();
        if (shouldInit) {
            mIsCreatedSessionAndInitForTesting = true;
            createSessionAndInit();
        }
    }

    /** Creates a session and initializes the schema type. */
    boolean createSessionAndInit() {
        if (mAppSearchSession != null) {
            return false;
        }

        if (sSkipInitializationForTesting) return true;

        // There are 3 steps for initialization:
        // 1) Set up a new app search session for tab donation and a global search session for
        //    checking device compatibility.
        // 2) Checks if the system has stored the consumer schema. This schema indicates the device
        //    is capable to use the Tabs from donation.
        // 3) Checks if the WebPage schema has been set for Tab donations.
        // If 2) failed, closes the app search session.
        mAppSearchSession = createAppSearchSession();
        mGlobalSearchSession = createGlobalSearchSession();
        if (mSkipSchemaCheck) {
            onConsumerSchemaSearched(/* success= */ true);
        } else {
            searchConsumerSchema(this::onConsumerSchemaSearched);
        }
        return true;
    }

    /** Creates a session asynchronously. */
    @SuppressLint("NewApi")
    private ListenableFuture<AppSearchSession> createAppSearchSession() {
        return PlatformStorage.createSearchSessionAsync(
                new PlatformStorage.SearchContext.Builder(mContext, mContext.getPackageName())
                        .build());
    }

    /** Creates a session asynchronously. */
    @SuppressLint("NewApi")
    private ListenableFuture<GlobalSearchSession> createGlobalSearchSession() {
        return PlatformStorage.createGlobalSearchSessionAsync(
                new PlatformStorage.GlobalSearchContext.Builder(mContext).build());
    }

    /**
     * Sets the document schema for the current session.
     *
     * @return false if the schema has been set before.
     */
    @SuppressWarnings("CheckResult")
    @RequiresNonNull({"mAppSearchSession", "mGlobalSearchSession"})
    boolean onConsumerSchemaSearched(boolean success) {
        assert mGlobalSearchSession != null;
        boolean ret = onConsumerSchemaSearchedImpl(success);

        // Closes the mGlobalSearchSession after querying the schema.
        Futures.transform(
                mGlobalSearchSession,
                session -> {
                    session.close();
                    mGlobalSearchSession = null;
                    return null;
                },
                AsyncTask.THREAD_POOL_EXECUTOR);

        return ret;
    }

    @SuppressLint({"CheckResult", "NewApi"})
    @RequiresNonNull("mAppSearchSession")
    @VisibleForTesting
    boolean onConsumerSchemaSearchedImpl(boolean success) {
        mIsDeviceCompatible = success;
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, success);

        mIsSchemaSet =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false);

        if (!mIsDeviceCompatible) {
            if (mIsSchemaSet) {
                // If WebPage schema has been set before while the device isn't capable for Tab
                // donations, clean up now.
                deleteAll(null);
                closeSession();
            }
            return false;
        }

        int schemaVersion = AuxiliarySearchUtils.getSchemaVersion();
        if (schemaVersion < AuxiliarySearchUtils.CURRENT_SCHEMA_VERSION) {
            mIsSchemaSet = false;
        }

        if (mIsSchemaSet) {
            // The WebPage schema only needs to be set once. Early exits if it has been set before.
            handlePendingDonations();
            return false;
        }

        Futures.transformAsync(
                mAppSearchSession,
                session -> {
                    SetSchemaRequest setSchemaRequest = buildSetSchemaRequest();
                    if (setSchemaRequest == null) {
                        return null;
                    }

                    ListenableFuture<SetSchemaResponse> responseFutureCallback =
                            session.setSchemaAsync(setSchemaRequest);
                    addRequestCallback(
                            responseFutureCallback,
                            (Callback<@Nullable SetSchemaResponse>)
                                    (response) -> onSetSchemaResponseAvailable(response),
                            UI_THREAD_EXECUTOR);
                    return responseFutureCallback;
                },
                AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    private @Nullable SetSchemaRequest buildSetSchemaRequest() {
        try {
            SetSchemaRequest.Builder requestBuilder =
                    new SetSchemaRequest.Builder()
                            .setForceOverride(true)
                            .addDocumentClasses(WebPage.class);
            AuxiliarySearchControllerFactory.getInstance()
                    .setSchemaTypeVisibilityForPackage(
                            (schemaClass, packageName, sha256Certificate) ->
                                    setDocumentClassVisibilityImpl(
                                            requestBuilder,
                                            schemaClass,
                                            packageName,
                                            sha256Certificate));
            return requestBuilder.build();
        } catch (AppSearchException e) {
            Log.i(TAG, "Failed to add document when building SetSchemaRequest.");
            return null;
        }
    }

    private void setDocumentClassVisibilityImpl(
            SetSchemaRequest.Builder requestBuilder,
            Class<?> schemaClass,
            String packageName,
            String sha256Certificate) {
        try {
            requestBuilder.setDocumentClassVisibilityForPackage(
                    schemaClass,
                    /* visible= */ true,
                    new PackageIdentifier(
                            packageName, new Signature(sha256Certificate).toByteArray()));
        } catch (AppSearchException e) {
            Log.i(TAG, "Failed to set document class visibility for package" + " %s.", packageName);
        }
    }

    @VisibleForTesting
    void onSetSchemaResponseAvailable(@Nullable SetSchemaResponse response) {
        if (response == null || !response.getMigrationFailures().isEmpty()) return;

        mIsSchemaSet = true;
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        AuxiliarySearchUtils.setSchemaVersion(AuxiliarySearchUtils.CURRENT_SCHEMA_VERSION);

        handlePendingDonations();
    }

    private void handlePendingDonations() {
        if (mPendingDocuments == null) return;

        // If there is any pending donation, donates the documents now.
        donateTabsImpl(mPendingDocuments, mPendingCallback);
        mPendingDocuments.clear();
        mPendingDocuments = null;
        mPendingCallback = null;
    }

    /**
     * Donates favicons. Only the tabs with favicons will be donated.
     *
     * @param entries The list of objects to donate.
     * @param entryToFaviconMap The map of <Entry, favicon>.
     */
    @VisibleForTesting
    public <T> void donateFavicons(
            List<T> entries, Map<T, Bitmap> entryToFaviconMap, Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<>();

        long currentTime = getCurrentTimeMillis();
        for (T entry : entries) {
            Bitmap favicon = entryToFaviconMap.get(entry);
            if (favicon != null) {
                docs.add(buildDocument(entry, favicon, /* counts= */ null, currentTime));
            }
        }

        assert !docs.isEmpty();

        donateTabsImpl(docs, callback);
    }

    /** Donates a list of data entries. */
    @VisibleForTesting
    public <T> void donateEntries(List<T> entries, int[] counts, Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<>();

        long currentTime = getCurrentTimeMillis();
        for (T entry : entries) {
            docs.add(buildDocument(entry, /* favicon= */ null, counts, currentTime));
        }

        donateTabsImpl(docs, callback);
    }

    /**
     * Donates tabs with favicons.
     *
     * @param entryToFaviconMap The map of tab with favicons.
     */
    @VisibleForTesting
    public <T> void donateEntries(Map<T, Bitmap> entryToFaviconMap, Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<>();

        long currentTime = getCurrentTimeMillis();
        for (Map.Entry<T, Bitmap> entry : entryToFaviconMap.entrySet()) {
            docs.add(
                    buildDocument(
                            entry.getKey(), entry.getValue(), /* counts= */ null, currentTime));
        }
        donateTabsImpl(docs, callback);
    }

    /** Creates a document for the given entry and favicon. */
    @VisibleForTesting
    <T> WebPage buildDocument(
            T entry, @Nullable Bitmap favicon, int @Nullable [] counts, long currentTime) {
        if (entry instanceof Tab tab) {
            String documentId = getDocumentId(AuxiliarySearchEntryType.TAB, tab.getId());
            if (counts != null) {
                counts[AuxiliarySearchEntryType.TAB] += 1;
            }
            WebPage.Builder builder = new WebPage.Builder(mNamespace, documentId);
            return buildDocumentImpl(
                    builder,
                    documentId,
                    tab.getUrl().getSpec(),
                    tab.getTitle(),
                    tab.getTimestampMillis(),
                    calculateDocumentTtlMs(
                            /* isTab= */ true, tab.getTimestampMillis(), currentTime),
                    /* score= */ 0,
                    SOURCE_TAB,
                    favicon);
        }

        if (entry instanceof AuxiliarySearchEntry auxiliarySearchEntry) {
            String documentId =
                    getDocumentId(AuxiliarySearchEntryType.TAB, auxiliarySearchEntry.getId());
            if (counts != null) {
                counts[AuxiliarySearchEntryType.TAB] += 1;
            }
            WebPage.Builder builder = new WebPage.Builder(mNamespace, documentId);
            return buildDocumentImpl(
                    builder,
                    documentId,
                    auxiliarySearchEntry.getUrl(),
                    auxiliarySearchEntry.getTitle(),
                    auxiliarySearchEntry.getLastAccessTimestamp(),
                    calculateDocumentTtlMs(
                            /* isTab= */ true,
                            auxiliarySearchEntry.getLastAccessTimestamp(),
                            currentTime),
                    /* score= */ 0,
                    SOURCE_TAB,
                    favicon);
        }

        AuxiliarySearchDataEntry dataEntry = (AuxiliarySearchDataEntry) entry;
        boolean isTab = dataEntry.type == AuxiliarySearchEntryType.TAB;
        int entryId = isTab ? dataEntry.tabId : dataEntry.visitId;
        String documentId = getDocumentId(dataEntry.type, entryId);
        if (counts != null) {
            counts[dataEntry.type] += 1;
        }
        // TODO(https://397457989): Creates a builder based on entry's type.
        WebPage.Builder builder = new WebPage.Builder(mNamespace, documentId);
        return buildDocumentImpl(
                builder,
                documentId,
                dataEntry.url.getSpec(),
                dataEntry.title,
                dataEntry.lastActiveTime,
                calculateDocumentTtlMs(isTab, dataEntry.lastActiveTime, currentTime),
                dataEntry.score,
                getSource(dataEntry.type),
                favicon);
    }

    private String getSource(@AuxiliarySearchEntryType int type) {
        switch (type) {
            case AuxiliarySearchEntryType.CUSTOM_TAB -> {
                return SOURCE_CUSTOM_TAB;
            }
            case AuxiliarySearchEntryType.TOP_SITE -> {
                return SOURCE_TOP_SITE;
            }
            default -> {
                return SOURCE_TAB;
            }
        }
    }

    @SuppressLint("UnsafeOptInUsageError")
    private WebPage buildDocumentImpl(
            WebPage.Builder builder,
            String documentId,
            String url,
            String title,
            long lastAccessTimestamp,
            long documentTtlMs,
            int score,
            String source,
            @Nullable Bitmap favicon) {
        byte[] faviconBytes = null;
        if (favicon != null) {
            faviconBytes = AuxiliarySearchUtils.bitmapToBytes(favicon);
        }

        builder.setUrl(url)
                .setName(title)
                .setCreationTimestampMillis(lastAccessTimestamp)
                .setDocumentTtlMillis(documentTtlMs)
                .setDocumentScore(score)
                .setSource(source);

        if (faviconBytes != null) {
            ImageObject faviconImage =
                    new ImageObject.Builder(mNamespace, documentId)
                            .setDocumentTtlMillis(documentTtlMs)
                            .setCreationTimestampMillis(lastAccessTimestamp)
                            .setBytes(faviconBytes)
                            .build();
            builder.setFavicon(faviconImage);
        }

        return builder.build();
    }

    /**
     * Implement Tab donations. If donation should be disabled, the donation list will be abandoned.
     * If the session hasn't been initialized, the list will be cached and executed after the
     * initialization is completed. The initialization process will clean up if donation should be
     * disabled.
     *
     * @param docs The documents to donate.
     * @param callback The callback to be called after donation is completed.
     */
    @SuppressLint("CheckResult")
    private void donateTabsImpl(List<WebPage> docs, @Nullable Callback<Boolean> callback) {
        if (mAppSearchSession == null) {
            return;
        }

        if (!initialized()) {
            // If the initialization hasn't been completed yet, cache the donation list.
            mPendingDocuments = docs;
            mPendingCallback = callback;
            return;
        }

        try {
            Futures.transformAsync(
                    mAppSearchSession,
                    (AsyncFunction<AppSearchSession, AppSearchBatchResult<String, Void>>)
                            session -> {
                                PutDocumentsRequest.Builder requestBuilder =
                                        new PutDocumentsRequest.Builder();
                                requestBuilder.addDocuments(docs);
                                PutDocumentsRequest request = requestBuilder.build();
                                ListenableFuture<AppSearchBatchResult<String, Void>>
                                        appSearchBatchResultCallback = session.putAsync(request);

                                addRequestCallback(
                                        appSearchBatchResultCallback,
                                        (Callback<@Nullable AppSearchBatchResult<String, Void>>)
                                                (batchResult) -> {
                                                    boolean isSuccess = false;
                                                    if (batchResult != null) {
                                                        Log.i(
                                                                TAG,
                                                                "successfulResults:"
                                                                        + batchResult
                                                                                .getSuccesses()
                                                                                .size()
                                                                        + ", failedResults:"
                                                                        + batchResult
                                                                                .getFailures()
                                                                                .size());
                                                        isSuccess =
                                                                batchResult.getSuccesses().size()
                                                                        == docs.size();
                                                    } else {
                                                        Log.i(TAG, "Failed to put documents.");
                                                    }

                                                    if (callback != null) {
                                                        callback.onResult(isSuccess);
                                                    }
                                                },
                                        UI_THREAD_EXECUTOR);
                                return appSearchBatchResultCallback;
                            },
                    UI_THREAD_EXECUTOR);
        } catch (Exception e) {
            Log.i(TAG, "Failed to donate documents.", e);
        }
    }

    /**
     * Removes all documents for auxiliary search based on namespace.
     *
     * @param onDeleteCompleteCallback The callback to be called when the deletion is completed.
     * @return whether it is possible to delete donated Tabs.
     */
    @SuppressLint("CheckResult")
    @VisibleForTesting
    public boolean deleteAll(@Nullable Callback<Boolean> onDeleteCompleteCallback) {
        if (mAppSearchSession == null) return false;

        SearchSpec spec = new SearchSpec.Builder().addFilterNamespaces(mNamespace).build();

        Futures.transformAsync(
                mAppSearchSession,
                session -> {
                    ListenableFuture<Void> result = session.removeAsync("", spec);
                    Futures.addCallback(
                            result,
                            new FutureCallback<Void>() {
                                @Override
                                public void onSuccess(@Nullable Void result) {
                                    Callback.runNullSafe(onDeleteCompleteCallback, true);
                                }

                                @Override
                                public void onFailure(Throwable t) {
                                    Callback.runNullSafe(onDeleteCompleteCallback, false);
                                }
                            },
                            AsyncTask.THREAD_POOL_EXECUTOR);
                    return result;
                },
                AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    /** Called when config in Tabs settings is changed. */
    @VisibleForTesting
    void onConfigChanged(boolean enabled, @Nullable Callback<Boolean> onDeleteCompleteCallback) {
        if (mSharedTabsWithOsState == enabled) return;

        mSharedTabsWithOsState = enabled;
        AuxiliarySearchUtils.setSharedTabsWithOs(enabled);
        if (enabled) {
            // Initializes the session now.
            createSessionAndInit();
        } else {
            // When disabled, remove all shared Tabs and closes the session.
            deleteAll(onDeleteCompleteCallback);
            closeSession();
        }
    }

    /** Closes the session. This is called when Tab donations is disabled. */
    @SuppressLint("CheckResult")
    private void closeSession() {
        if (mAppSearchSession != null) {
            Futures.transform(
                    mAppSearchSession,
                    session -> {
                        session.close();
                        mAppSearchSession = null;
                        return null;
                    },
                    AsyncTask.THREAD_POOL_EXECUTOR);
        }

        if (mPendingDocuments != null) {
            mPendingDocuments.clear();
            mPendingDocuments = null;
        }

        if (mPendingCallback != null) {
            mPendingCallback.onResult(false);
        }
    }

    @VisibleForTesting
    public static String getDocumentId(int type, int id) {
        switch (type) {
            case AuxiliarySearchEntryType.TAB:
                return TAB_PREFIX + id;
            case AuxiliarySearchEntryType.CUSTOM_TAB:
                return CUSTOM_TAB_PREFIX + id;
            case AuxiliarySearchEntryType.TOP_SITE:
                return TOP_SITE_PREFIX + id;
            default:
                assert false : "The type isn't supported: " + type;
                return assumeNonNull(null);
        }
    }

    /** Returns the calculated TTL for a donated document in MS. */
    @VisibleForTesting
    public long calculateDocumentTtlMs(boolean isTab, long creationTime, long currentTime) {
        return currentTime
                - creationTime
                + (isTab ? getTabDocumentTtlMs() : getHistoryDocumentTtlMs());
    }

    /** Returns the donated document's TTL in MS. */
    @VisibleForTesting
    public long getTabDocumentTtlMs() {
        if (mTabTtlMillis == null) {
            mTabTtlMillis =
                    TimeUnit.HOURS.toMillis(sAndroidAppIntegrationV2ContentTtlHours.getValue());
        }

        return mTabTtlMillis;
    }

    @VisibleForTesting
    public long getHistoryDocumentTtlMs() {
        if (mHistoryTtlMillis == null) {
            mHistoryTtlMillis =
                    TimeUnit.HOURS.toMillis(
                            sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours.getValue());
        }

        return mHistoryTtlMillis;
    }

    private static <T extends @Nullable Object> void addRequestCallback(
            ListenableFuture<T> result, Callback<@Nullable T> callback, Executor executor) {
        Futures.addCallback(
                result,
                new FutureCallback<@Nullable T>() {
                    @Override
                    public void onSuccess(@Nullable T result) {
                        callback.onResult(result);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        callback.onResult(null);
                    }
                },
                executor);
    }

    /**
     * Returns whether the donor is able to donate Tabs. Returns true if 1) the settings is enabled,
     * 2) the session isn't closed and 3) the device is compatible or the compatibility check is in
     * progress.
     */
    boolean canDonate() {
        if (!mSharedTabsWithOsState || mAppSearchSession == null) return false;

        // If mIsDeviceCompatible is null, it means the checking of device compatibility is still
        // working in progress. In this case, it is safe to let the caller to send a donation list,
        // and this donor will either close the session or continue to donate the pending list when
        // the check of mIsDeviceCompatible is done. The follow tasks are handled in {@link
        // AuxiliarySearchDonor#onConsumerSchemaSearchedImpl(boolean)}.
        if (Boolean.FALSE.equals(mIsDeviceCompatible)) return false;

        return true;
    }

    /** Returns whether the donor is fully initialized. */
    boolean initialized() {
        return mIsSchemaSet && mIsDeviceCompatible != null;
    }

    /**
     * Searches whether the device supports Tab donation feature.
     *
     * @param callback The callback to be called after the query is completed.
     */
    @SuppressWarnings({"CheckResult", "UnsafeOptInUsageError", "RequiresFeature"})
    @RequiresNonNull("mGlobalSearchSession")
    private void searchConsumerSchema(Callback<Boolean> callback) {
        String supportedPackageName =
                AuxiliarySearchControllerFactory.getInstance().getSupportedPackageName();
        if (supportedPackageName == null) {
            callback.onResult(false);
            return;
        }

        SearchSpec searchSpec =
                new SearchSpec.Builder()
                        .addFilterSchemas(SCHEMA)
                        .addFilterPackageNames(supportedPackageName)
                        .build();

        SearchQueryChecker searchQueryChecker =
                searchResult -> {
                    GenericDocument genericDocument = searchResult.getGenericDocument();
                    try {
                        GlobalSearchApplicationInfo info =
                                genericDocument.toDocumentClass(GlobalSearchApplicationInfo.class);
                        if (info.getApplicationType()
                                        == GlobalSearchApplicationInfo.APPLICATION_TYPE_CONSUMER
                                && info.getSchemaTypes().contains(SCHEMA_WEBPAGE)) {
                            return true;
                        }
                    } catch (AppSearchException e) {
                        Log.i(
                                TAG,
                                "Failed to convert GenericDocument to"
                                        + " GlobalSearchApplicationInfo");
                    }
                    return false;
                };

        Futures.transformAsync(
                mGlobalSearchSession,
                session ->
                        processSearchResults(
                                session.search(/* queryExpression= */ "", searchSpec),
                                callback,
                                searchQueryChecker),
                AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private ListenableFuture<Void> processSearchResults(
            SearchResults searchResults,
            Callback<Boolean> callback,
            SearchQueryChecker searchQueryChecker) {
        if (sSkipInitializationForTesting) {
            callback.onResult(false);
            return (ListenableFuture<Void>) Futures.immediateVoidFuture();
        }

        return Futures.transformAsync(
                searchResults.getNextPageAsync(),
                page -> iterateSearchResults(searchResults, page, callback, searchQueryChecker),
                AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    ListenableFuture<Void> iterateSearchResults(
            SearchResults searchResults,
            List<SearchResult> page,
            Callback<Boolean> callback,
            SearchQueryChecker searchQueryChecker) {
        if (page.isEmpty()) {
            searchResults.close();
            callback.onResult(false);
            return (ListenableFuture<Void>) Futures.immediateVoidFuture();
        }

        for (int i = 0; i < page.size(); i++) {
            if (searchQueryChecker.isSuccess(page.get(i))) {
                callback.onResult(true);
                searchResults.close();
                return (ListenableFuture<Void>) Futures.immediateVoidFuture();
            }
        }

        return processSearchResults(searchResults, callback, searchQueryChecker);
    }

    @SuppressLint("CheckResult")
    public void searchDonationResultsForTesting(Callback<List<SearchResult>> callback) {
        assert mAppSearchSession != null;

        SearchSpec searchSpec = new SearchSpec.Builder().addFilterNamespaces(mNamespace).build();

        ListenableFuture<SearchResults> searchFutureCallback =
                Futures.transform(
                        mAppSearchSession,
                        session -> session.search("", searchSpec),
                        UI_THREAD_EXECUTOR);

        addRequestCallback(
                searchFutureCallback,
                (Callback<@Nullable SearchResults>)
                        (searchResults) -> {
                            if (searchResults != null) {
                                Futures.transform(
                                        searchResults.getNextPageAsync(),
                                        page -> {
                                            callback.onResult(page);
                                            return null;
                                        },
                                        UI_THREAD_EXECUTOR);
                            } else {
                                Log.i(TAG, "Failed to search documents.");
                            }
                        },
                UI_THREAD_EXECUTOR);
    }

    @VisibleForTesting
    boolean isShareTabsWithOsEnabledKeyExist() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.contains(ChromePreferenceKeys.SHARING_TABS_WITH_OS);
    }

    /** Returns the current time in milliseconds. */
    private long getCurrentTimeMillis() {
        // Uses TimeUtils.currentTimeMillis() since the last visited time of the document is got by
        // using base::Time::InMillisecondsSinceUnixEpoch() in native.
        return TimeUtils.currentTimeMillis();
    }

    public boolean getIsSchemaSetForTesting() {
        return mIsSchemaSet;
    }

    public @Nullable List<WebPage> getPendingDocumentsForTesting() {
        return mPendingDocuments;
    }

    public void setPendingDocumentsForTesting(List<WebPage> docs) {
        mPendingDocuments = docs;
    }

    public void setPendingCallbackForTesting(Callback<Boolean> pendingCallbackForTesting) {
        Callback<Boolean> oldPendingCallback = mPendingCallback;
        mPendingCallback = pendingCallbackForTesting;
        ResettersForTesting.register(() -> mPendingCallback = oldPendingCallback);
    }

    public static void setSkipInitializationForTesting(boolean skipInitializationForTesting) {
        sSkipInitializationForTesting = skipInitializationForTesting;
        ResettersForTesting.register(() -> sSkipInitializationForTesting = false);
    }

    public void resetSchemaSetForTesting() {
        mIsSchemaSet = false;
    }

    public boolean getSharedTabsWithOsStateForTesting() {
        return mSharedTabsWithOsState;
    }

    public void setSharedTabsWithOsStateForTesting(boolean sharedTabsWithOsState) {
        boolean oldValue = mSharedTabsWithOsState;
        mSharedTabsWithOsState = sharedTabsWithOsState;
        ResettersForTesting.register(() -> mSharedTabsWithOsState = oldValue);
    }

    public void setAppSearchSessionForTesting(
            ListenableFuture<AppSearchSession> sessionForTesting) {
        ListenableFuture<AppSearchSession> oldSession = mAppSearchSession;
        mAppSearchSession = sessionForTesting;
        ResettersForTesting.register(() -> mAppSearchSession = oldSession);
    }

    public static AuxiliarySearchDonor createDonorForTesting() {
        return new AuxiliarySearchDonor();
    }

    public boolean getIsCreatedSessionAndInitForTesting() {
        return mIsCreatedSessionAndInitForTesting;
    }
}
