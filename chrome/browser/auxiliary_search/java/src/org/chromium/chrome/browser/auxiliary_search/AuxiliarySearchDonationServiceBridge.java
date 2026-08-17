// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.VisibleForTesting;
import androidx.appsearch.app.AppSearchBatchResult;
import androidx.appsearch.app.AppSearchSchema;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.GenericDocument;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.builtintypes.Account;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.MoreExecutors;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.AlwaysInline;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.io.Closeable;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Java bridge to allow C++'s `AuxiliarySearchDonationService` to donate browsing history to other
 * apps via AppSearch.
 */
@NullMarked
class AuxiliarySearchDonationServiceBridge implements Closeable {
    @VisibleForTesting static final String TAG = "AuxSearchDonation";

    // Differs from `AuxiliarySearchDonor`, which uses the package name as both the database name
    // and namespace.
    @VisibleForTesting static final String DATABASE_NAME = "browsing_data";
    @VisibleForTesting static final String HISTORY_NAMESPACE = "History";
    @VisibleForTesting static final long HISTORY_DOCUMENT_TTL_MILLIS = TimeUnit.HOURS.toMillis(24);

    /**
     * Name of the schema that extends `androidx.appsearch.builtintypes.WebPage` to include
     * additional properties required by receiving apps.
     *
     * <p>See {@link #createSetSchemaRequest()} for the source of truth for this schema.
     */
    @VisibleForTesting static final String CHROME_WEB_PAGE_SCHEMA_NAME = "ChromeWebPage";

    // Additional properties:
    /** An optional {@link androidx.appsearch.builtintypes.Account}. */
    @VisibleForTesting static final String ACCOUNT_PROPERTY_NAME = "chromeAccount";

    // `androidx.appsearch.builtintypes.Account` doesn't expose its schema name as a constant.
    // Hard-code it based on the source of truth from AndroidX:
    // https://cs.android.com/search?q=f:androidx%2Fappsearch%2Fbuiltintypes%2FAccount.java
    @VisibleForTesting static final String BUILTIN_ACCOUNT_SCHEMA_NAME = "builtin:Account";
    // Account type for Google accounts. See
    // https://developer.android.com/reference/android/provider/ContactsContract.RawContacts#:~:text=com.google
    // for reference.
    @VisibleForTesting static final String ACCOUNT_TYPE_GOOGLE = "com.google";

    // Future which holds the `AppSearchSession` after initialization and any subsequent schema
    // updates. "Awaiting" this will ensure the session is initialized and the schema is set.
    //
    // Null iff the Android version does not support the AppSearch APIs OR there are no available
    // apps to donate to (e.g. in a build without an `AuxiliarySearchHooks` implementation).
    @VisibleForTesting @Nullable ListenableFuture<AppSearchSession> mSessionFuture;

    @CalledByNative
    static boolean isBrowsingDataDonationSupported() {
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        return hooks != null && hooks.isBrowsingDataDonationSupported();
    }

    @CalledByNative
    public AuxiliarySearchDonationServiceBridge(boolean isBrowsingDataDonationEnabled) {
        // As this function is @CalledByNative, we must add a `getPackagesForBrowsingDataVisibility`
        // check here to prevent AppSearch code (schemas) from being compiled into open source APKs.
        Set<PackageIdentifier> packages = getPackagesForBrowsingDataVisibility();
        if (packages == null) {
            mSessionFuture = null;
            return;
        }

        mSessionFuture =
                AppSearchStorageFactory.getInstance().createSearchSessionAsync(DATABASE_NAME);
        setSchema(packages, isBrowsingDataDonationEnabled);
    }

    @CalledByNative
    public void donateHistory(
            @JniType("std::vector<AuxiliarySearchDonationService::HistoryData>")
                    List<WebPage> pages,
            @JniType("std::optional<CoreAccountInfo>") @Nullable CoreAccountInfo coreAccountInfo) {
        // As this function is @CalledByNative, we must add a `getPackagesForBrowsingDataVisibility`
        // check here to prevent AppSearch code from being compiled into open source APKs.
        if (getPackagesForBrowsingDataVisibility() == null
                || mSessionFuture == null
                || pages.isEmpty()) {
            return;
        }
        Account account = fromCoreAccountInfo(coreAccountInfo);

        var _ =
                Futures.transformAsync(
                        mSessionFuture,
                        session -> {
                            var builder = new PutDocumentsRequest.Builder();
                            for (WebPage page : pages) {
                                // `GenericDocument.fromDocumentClass` can throw "if no factory for
                                // this document class could be found on the classpath", but that
                                // should never happen. Call it from the top-level `transformAsync`
                                // lambda, instead of a stream map, to ensure that if an exception
                                // _is_ thrown, it is caught by `transformAsync`.
                                GenericDocument webPageDoc =
                                        GenericDocument.fromDocumentClass(page);
                                var extendedDocBuilder =
                                        new GenericDocument.Builder<>(webPageDoc)
                                                .setSchemaType(CHROME_WEB_PAGE_SCHEMA_NAME);
                                if (account != null) {
                                    extendedDocBuilder.setPropertyDocument(
                                            ACCOUNT_PROPERTY_NAME,
                                            GenericDocument.fromDocumentClass(account));
                                }
                                builder.addGenericDocuments(extendedDocBuilder.build());
                            }
                            ListenableFuture<AppSearchBatchResult<String, Void>> putFuture =
                                    session.putAsync(builder.build());
                            // WARNING: Do not log URLs here, including document IDs which contain
                            // URLs. See docs/android_logging.md for more information.
                            Futures.addCallback(
                                    putFuture,
                                    new FutureCallback<AppSearchBatchResult<String, Void>>() {
                                        @Override
                                        public void onSuccess(
                                                AppSearchBatchResult<String, Void> result) {
                                            if (!result.isSuccess()) {
                                                Log.w(
                                                        TAG,
                                                        "Failed to donate documents: %d"
                                                                + " failure(s).",
                                                        result.getFailures().size());
                                            }
                                        }

                                        @Override
                                        public void onFailure(Throwable t) {
                                            Log.w(TAG, "Failed to donate history to AppSearch.", t);
                                        }
                                    },
                                    MoreExecutors.directExecutor());
                            return putFuture;
                        },
                        MoreExecutors.directExecutor());
    }

    @CalledByNative
    @Override
    public void close() {
        // As this function is @CalledByNative, we must add a `getPackagesForBrowsingDataVisibility`
        // check here to prevent AppSearch code (`session.close()`) from being compiled into open
        // source APKs.
        if (getPackagesForBrowsingDataVisibility() == null || mSessionFuture == null) {
            return;
        }
        var _ =
                Futures.transform(
                        mSessionFuture,
                        session -> {
                            session.close();
                            return null;
                        },
                        MoreExecutors.directExecutor());
    }

    // Differs from `AuxiliarySearchDonor`, which only calls `setSchemaAsync` if the last set
    // schema version (stored in prefs) differs from the "current" schema version.
    //
    // From the `AppSearchSession#setSchemaAsync` documentation:
    // > Upon creating an `AppSearchSession`, `setSchemaAsync` should be called.
    // > If the schema needs to be updated, or it has not been previously set,
    // > then the provided schema will be saved and persisted to disk.
    // > Otherwise, `setSchemaAsync` is handled efficiently as a no-op call.
    @CalledByNative
    public void setSchema(boolean isBrowsingDataDonationEnabled) {
        // As this function is @CalledByNative, we must add a `getPackagesForBrowsingDataVisibility`
        // check here to prevent AppSearch code (schemas) from being compiled into open source APKs.
        Set<PackageIdentifier> packages = getPackagesForBrowsingDataVisibility();
        if (packages == null) {
            return;
        }
        setSchema(packages, isBrowsingDataDonationEnabled);
    }

    private void setSchema(Set<PackageIdentifier> packages, boolean isBrowsingDataDonationEnabled) {
        if (mSessionFuture == null) {
            return;
        }
        mSessionFuture =
                Futures.transformAsync(
                        mSessionFuture,
                        session -> {
                            ListenableFuture<SetSchemaResponse> schemaFuture =
                                    session.setSchemaAsync(
                                            createSetSchemaRequest(
                                                    packages, isBrowsingDataDonationEnabled));
                            Futures.addCallback(
                                    schemaFuture,
                                    new FutureCallback<SetSchemaResponse>() {
                                        @Override
                                        public void onSuccess(SetSchemaResponse result) {}

                                        @Override
                                        public void onFailure(Throwable t) {
                                            Log.w(
                                                    TAG,
                                                    "Failed to update schema visibility in"
                                                            + " AppSearch.",
                                                    t);
                                        }
                                    },
                                    MoreExecutors.directExecutor());
                            return Futures.transform(
                                    schemaFuture, _ -> session, MoreExecutors.directExecutor());
                        },
                        MoreExecutors.directExecutor());
    }

    @CalledByNative
    public static WebPage createHistoryDocument(
            @JniType("std::string") String id,
            @JniType("std::string") String url,
            @JniType("std::u16string") String title,
            long lastVisited) {
        return new WebPage.Builder(HISTORY_NAMESPACE, id)
                .setUrl(url)
                .setName(title)
                .setCreationTimestampMillis(lastVisited)
                .setDocumentTtlMillis(HISTORY_DOCUMENT_TTL_MILLIS)
                .build();
    }

    private static @Nullable Account fromCoreAccountInfo(
            @Nullable CoreAccountInfo coreAccountInfo) {
        if (coreAccountInfo == null) {
            return null;
        }
        String gaiaId = coreAccountInfo.getGaiaId().toString();
        return new Account.Builder(HISTORY_NAMESPACE, gaiaId)
                .setAccountId(gaiaId)
                .setAccountName(coreAccountInfo.getEmail())
                .setAccountType(ACCOUNT_TYPE_GOOGLE)
                .build();
    }

    // Returns the list of known consumer packages for browsing data. This is statically guaranteed
    // to be null by R8 if the current build does not have any known consumers (e.g. open source
    // builds).
    // All uses of AppSearch code MUST be guarded behind null-checking this return value to ensure
    // that AppSearch code does not significantly bloat open source APK binary size - this will be
    // checked by the android-binary-size trybot. See
    // docs/speed/binary_size/android_binary_size_trybot.md for more details.
    @AlwaysInline
    private static @Nullable Set<PackageIdentifier> getPackagesForBrowsingDataVisibility() {
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        if (hooks == null) {
            return null;
        }
        Set<PackageIdentifier> packages = hooks.getPackagesForBrowsingDataVisibility();
        return packages.isEmpty() ? null : packages;
    }

    @VisibleForTesting
    static SetSchemaRequest createSetSchemaRequest(
            Set<PackageIdentifier> packages, boolean isBrowsingDataDonationEnabled)
            throws AppSearchException {
        var builder = new SetSchemaRequest.Builder();
        // Delete old documents incompatible with the new schema.
        builder.setForceOverride(true);
        // `addSchemas` does not include parent or nested property schemas, but `addDocumentClasses`
        // DOES include them. Use that to add all schemas used by `extendedSchema`.
        builder.addDocumentClasses(WebPage.class, Account.class);

        AppSearchSchema webpageSchema = AppSearchSchema.fromDocumentClass(WebPage.class);
        AppSearchSchema extendedSchema =
                new AppSearchSchema.Builder(webpageSchema)
                        .setSchemaType(CHROME_WEB_PAGE_SCHEMA_NAME)
                        .addParentType(WebPage.SCHEMA_NAME)
                        .addProperty(
                                new AppSearchSchema.DocumentPropertyConfig.Builder(
                                                ACCOUNT_PROPERTY_NAME, BUILTIN_ACCOUNT_SCHEMA_NAME)
                                        .setCardinality(
                                                AppSearchSchema.PropertyConfig.CARDINALITY_OPTIONAL)
                                        .setShouldIndexNestedProperties(true)
                                        .build())
                        .build();
        builder.addSchemas(extendedSchema);
        // Schema visibility is for the underlying document, so there's no need to set it on parent
        // schemas too as we only store `ChromeWebPage` and no other schemas.
        builder.setSchemaTypeDisplayedBySystem(CHROME_WEB_PAGE_SCHEMA_NAME, /* displayed= */ false);

        if (isBrowsingDataDonationEnabled) {
            for (PackageIdentifier packageIdentifier : packages) {
                builder.setSchemaTypeVisibilityForPackage(
                        CHROME_WEB_PAGE_SCHEMA_NAME, /* visible= */ true, packageIdentifier);
            }
        }
        return builder.build();
    }
}
