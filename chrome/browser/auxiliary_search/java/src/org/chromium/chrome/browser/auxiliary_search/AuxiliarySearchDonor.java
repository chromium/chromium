// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationV2ContentTtlHours;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.Signature;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appsearch.app.AppSearchBatchResult;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SearchResult;
import androidx.appsearch.app.SearchResults;
import androidx.appsearch.app.SearchSpec;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.builtintypes.ImageObject;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;
import androidx.appsearch.platformstorage.PlatformStorage;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
public class AuxiliarySearchDonor {

    /** Callback to set schema visibilities for package names. */
    interface SetDocumentClassVisibilityForPackageCallback {

        void setDocumentClassVisibility(String packageName, String sha256Certificate);
    }

    private static final String TAG = "AuxiliarySearchDonor";
    private static final String TAB_PREFIX = "Tab-";

    private static final Executor UI_THREAD_EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.UI_DEFAULT, r);
    private static boolean sSkipInitializationForTesting;

    private final Context mContext;
    private final String mNamespace;
    private ListenableFuture<AppSearchSession> mAppSearchSession;
    private Long mTtlMillis;
    private boolean mIsSchemaSet;
    private List<WebPage> mPendingDocuments;
    private Callback<Boolean> mPendingCallback;

    /**
     * @param context The application context.
     */
    public AuxiliarySearchDonor(@NonNull Context context) {
        mContext = context;
        mNamespace = mContext.getPackageName();
    }

    /** Creates a session and initializes the schema type. */
    void createSessionAndInit() {
        if (sSkipInitializationForTesting) return;

        if (mAppSearchSession != null) {
            return;
        }

        mAppSearchSession = createAppSearchSession();
        maySetSchema();
    }

    /** Creates a session asynchronously. */
    @SuppressLint("NewApi")
    private ListenableFuture<AppSearchSession> createAppSearchSession() {
        return PlatformStorage.createSearchSessionAsync(
                new PlatformStorage.SearchContext.Builder(mContext, mContext.getPackageName())
                        .build());
    }

    /**
     * Sets the document schema for the current session.
     *
     * @return false if the schema has been set before.
     */
    @SuppressLint({"CheckResult", "NewApi"})
    @VisibleForTesting
    boolean maySetSchema() {
        mIsSchemaSet =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false);
        if (mIsSchemaSet) return false;

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
                            (response) ->
                                    onSetSchemaResponseAvailable((SetSchemaResponse) response),
                            UI_THREAD_EXECUTOR);
                    return responseFutureCallback;
                },
                AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    @NonNull
    private SetSchemaRequest buildSetSchemaRequest() {
        try {
            SetSchemaRequest.Builder requestBuilder =
                    new SetSchemaRequest.Builder()
                            .setForceOverride(true)
                            .addDocumentClasses(WebPage.class);
            AuxiliarySearchControllerFactory.getInstance()
                    .setSchemaTypeVisibilityForPackage(
                            (packageName, sha256Certificate) -> {
                                try {
                                    requestBuilder.setDocumentClassVisibilityForPackage(
                                            WebPage.class,
                                            /* visible= */ true,
                                            new PackageIdentifier(
                                                    packageName,
                                                    new Signature(sha256Certificate)
                                                            .toByteArray()));
                                } catch (AppSearchException e) {
                                    Log.i(
                                            TAG,
                                            "Failed to set document class visibility for package"
                                                    + " %s.",
                                            packageName);
                                }
                            });
            return requestBuilder.build();
        } catch (AppSearchException e) {
            Log.i(TAG, "Failed to add document when building SetSchemaRequest.");
            return null;
        }
    }

    @VisibleForTesting
    void onSetSchemaResponseAvailable(@NonNull SetSchemaResponse response) {
        if (response == null || !response.getMigrationFailures().isEmpty()) return;

        mIsSchemaSet = true;
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);

        // If there is any pending donation, donates the documents now.
        if (mPendingDocuments != null) {
            donateTabsImpl(mPendingDocuments, mPendingCallback);
            mPendingDocuments.clear();
            mPendingDocuments = null;
            mPendingCallback = null;
        }
    }

    /**
     * Donates favicons. Only the tabs with favicons will be donated.
     *
     * @param entries The list of {@link AuxiliarySearchEntry} object which contains a Tab's data.
     * @param tabIdToFaviconMap The map of <TabId, favicon>.
     */
    @VisibleForTesting
    public void donateFavicons(
            @NonNull List<AuxiliarySearchEntry> entries,
            @NonNull Map<Integer, Bitmap> tabIdToFaviconMap,
            @NonNull Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<WebPage>();

        for (AuxiliarySearchEntry entry : entries) {
            Bitmap favicon = tabIdToFaviconMap.get(entry.getId());
            if (favicon != null) {
                docs.add(
                        buildDocument(
                                entry.getId(),
                                entry.getUrl(),
                                entry.getTitle(),
                                entry.getLastAccessTimestamp(),
                                favicon));
            }
        }

        assert !docs.isEmpty();

        donateTabsImpl(docs, callback);
    }

    /** Donates a list of tabs. */
    @VisibleForTesting
    public void donateTabs(@NonNull List<Tab> tabs, @NonNull Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<WebPage>();

        for (Tab tab : tabs) {
            docs.add(
                    buildDocument(
                            tab.getId(),
                            tab.getUrl().getSpec(),
                            tab.getTitle(),
                            tab.getTimestampMillis(),
                            null));
        }

        donateTabsImpl(docs, callback);
    }

    /**
     * Donates tabs with favicons.
     *
     * @param tabToFaviconMap The map of tab with favicons.
     */
    @VisibleForTesting
    public void donateTabs(
            @NonNull Map<Tab, Bitmap> tabToFaviconMap, @NonNull Callback<Boolean> callback) {
        List<WebPage> docs = new ArrayList<WebPage>();

        for (Map.Entry<Tab, Bitmap> entry : tabToFaviconMap.entrySet()) {
            Tab tab = entry.getKey();
            docs.add(
                    buildDocument(
                            tab.getId(),
                            tab.getUrl().getSpec(),
                            tab.getTitle(),
                            tab.getTimestampMillis(),
                            entry.getValue()));
        }
        donateTabsImpl(docs, callback);
    }

    @VisibleForTesting
    WebPage buildDocument(
            int id,
            @NonNull String url,
            @NonNull String title,
            long lastAccessTimestamp,
            @Nullable Bitmap favicon) {
        String documentId = getDocumentId(id);
        byte[] faviconBytes = null;
        if (favicon != null) {
            faviconBytes = AuxiliarySearchUtils.bitmapToBytes(favicon);
        }

        WebPage.Builder builder =
                new WebPage.Builder(mNamespace, documentId)
                        .setUrl(url)
                        .setName(title)
                        .setCreationTimestampMillis(lastAccessTimestamp)
                        .setDocumentTtlMillis(getDocumentTtlMs());

        if (faviconBytes != null) {
            ImageObject faviconImage =
                    new ImageObject.Builder(mNamespace, documentId)
                            .setDocumentTtlMillis(getDocumentTtlMs())
                            .setCreationTimestampMillis(lastAccessTimestamp)
                            .setBytes(faviconBytes)
                            .build();
            builder.setFavicon(faviconImage);
        }

        return builder.build();
    }

    @SuppressLint("CheckResult")
    private void donateTabsImpl(@NonNull List<WebPage> docs, @Nullable Callback<Boolean> callback) {
        if (!mIsSchemaSet) {
            // If the schema hasn't been set yet, cache the donation list.
            mPendingDocuments = docs;
            mPendingCallback = callback;
            return;
        }

        // This is only true for tests.
        if (mAppSearchSession == null) {
            return;
        }

        try {
            Futures.transformAsync(
                    mAppSearchSession,
                    session -> {
                        PutDocumentsRequest.Builder requestBuilder =
                                new PutDocumentsRequest.Builder();
                        requestBuilder.addDocuments(docs);
                        PutDocumentsRequest request = requestBuilder.build();
                        ListenableFuture<AppSearchBatchResult<String, Void>>
                                appSearchBatchResultCallback = session.putAsync(request);

                        addRequestCallback(
                                appSearchBatchResultCallback,
                                (batchResult) -> {
                                    boolean isSuccess = false;
                                    if (batchResult != null) {
                                        Log.i(
                                                TAG,
                                                "successfulResults:"
                                                        + batchResult.getSuccesses().size()
                                                        + ", failedResults:"
                                                        + batchResult.getFailures().size());
                                        isSuccess =
                                                batchResult.getSuccesses().size() == docs.size();
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

    /** Removes all tabs for auxiliary search based on namespace. */
    @SuppressLint("CheckResult")
    @VisibleForTesting
    public void deleteAllTabs(@NonNull Callback<Boolean> onDeleteCompleteCallback) {
        SearchSpec spec = new SearchSpec.Builder().addFilterNamespaces(mNamespace).build();

        Futures.transformAsync(
                mAppSearchSession,
                session -> {
                    ListenableFuture<Void> result = session.removeAsync("", spec);
                    Futures.addCallback(
                            result,
                            new FutureCallback<Void>() {
                                @Override
                                public void onSuccess(Void result) {
                                    onDeleteCompleteCallback.onResult(true);
                                }

                                @Override
                                public void onFailure(Throwable t) {
                                    onDeleteCompleteCallback.onResult(false);
                                }
                            },
                            AsyncTask.THREAD_POOL_EXECUTOR);
                    return result;
                },
                AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Closes the session. */
    @SuppressLint("CheckResult")
    @VisibleForTesting
    public void destroy() {
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
    public static String getDocumentId(int id) {
        return TAB_PREFIX + id;
    }

    /** Returns the donated document's TTL in MS. */
    @VisibleForTesting
    public long getDocumentTtlMs() {
        if (mTtlMillis == null) {
            mTtlMillis =
                    TimeUnit.HOURS.toMillis(sAndroidAppIntegrationV2ContentTtlHours.getValue());
        }

        return mTtlMillis;
    }

    private static <T> void addRequestCallback(
            @NonNull ListenableFuture<T> result,
            @Nullable Callback<T> callback,
            Executor executor) {
        Futures.addCallback(
                result,
                new FutureCallback<T>() {
                    @Override
                    public void onSuccess(T result) {
                        callback.onResult(result);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        callback.onResult(null);
                    }
                },
                executor);
    }

    @SuppressLint("CheckResult")
    public void searchDonationResultsForTesting(Callback<List<SearchResult>> callback) {
        SearchSpec searchSpec = new SearchSpec.Builder().addFilterNamespaces(mNamespace).build();

        ListenableFuture<SearchResults> searchFutureCallback =
                Futures.transform(
                        mAppSearchSession,
                        session -> session.search("", searchSpec),
                        UI_THREAD_EXECUTOR);

        addRequestCallback(
                searchFutureCallback,
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

    public boolean getIsSchemaSetForTesting() {
        return mIsSchemaSet;
    }

    public List<WebPage> getPendingDocumentsForTesting() {
        return mPendingDocuments;
    }

    public void setPendingDocumentsForTesting(List<WebPage> docs) {
        mPendingDocuments = docs;
    }

    public static void setSkipInitializationForTesting(boolean skipInitializationForTesting) {
        sSkipInitializationForTesting = skipInitializationForTesting;
        ResettersForTesting.register(() -> sSkipInitializationForTesting = false);
    }
}
