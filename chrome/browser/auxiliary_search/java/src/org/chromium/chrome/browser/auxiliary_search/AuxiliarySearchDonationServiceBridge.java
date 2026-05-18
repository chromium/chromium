// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.MoreExecutors;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Java bridge to allow C++'s `AuxiliarySearchDonationService` to donate browsing history to other
 * apps via AppSearch.
 */
@NullMarked
class AuxiliarySearchDonationServiceBridge {
    // Differs from `AuxiliarySearchDonor`, which uses the package name as both the database name
    // and namespace.
    @VisibleForTesting static final String DATABASE_NAME = "browsing_history";
    @VisibleForTesting static final String HISTORY_NAMESPACE = "History";
    @VisibleForTesting static final long HISTORY_DOCUMENT_TTL_MILLIS = TimeUnit.HOURS.toMillis(24);

    // Future which holds the `AppSearchSession` after initialization.
    // "Awaiting" this will ensure the session is initialized and the schema is set.
    //
    // Null iff the Android version does not support the AppSearch APIs OR there are no available
    // apps to donate to (e.g. in a build without an `AuxiliarySearchHooks` implementation).
    @VisibleForTesting final @Nullable ListenableFuture<AppSearchSession> mSessionFuture;

    @CalledByNative
    public AuxiliarySearchDonationServiceBridge() {
        mSessionFuture = setUpSessionFuture();
    }

    @CalledByNative
    public void donateHistory(
            @JniType("std::vector<AuxiliarySearchDonationService::HistoryData>")
                    List<WebPage> pages) {
        if (mSessionFuture == null || pages.isEmpty()) {
            return;
        }

        var unused =
                Futures.transformAsync(
                        mSessionFuture,
                        session ->
                                session.putAsync(
                                        new PutDocumentsRequest.Builder()
                                                .addDocuments(pages)
                                                .build()),
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

    private static @Nullable ListenableFuture<AppSearchSession> setUpSessionFuture() {
        // Check for available consumer apps to ensure this gets optimised out by R8 if it can
        // statically determine that there are no consumers of this data.
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        if (hooks == null || hooks.getPackagesForBrowsingDataVisibility().isEmpty()) {
            return null;
        }

        AppSearchStorageFactory factory = AppSearchStorageFactory.getInstance();

        ListenableFuture<AppSearchSession> sessionFuture =
                factory.createSearchSessionAsync(DATABASE_NAME);

        if (sessionFuture == null) {
            // AppSearch is not available on this device.
            return null;
        }

        // Differs from `AuxiliarySearchDonor`, which only calls `setSchemaAsync` if the last set
        // schema version (stored in prefs) differs from the "current" schema version.
        //
        // From the `AppSearchSession#setSchemaAsync` documentation:
        // > Upon creating an `AppSearchSession`, `setSchemaAsync` should be called.
        // > If the schema needs to be updated, or it has not been previously set,
        // > then the provided schema will be saved and persisted to disk.
        // > Otherwise, `setSchemaAsync` is handled efficiently as a no-op call.
        return Futures.transformAsync(
                sessionFuture,
                session ->
                        Futures.transform(
                                session.setSchemaAsync(createSetSchemaRequest()),
                                unusedResponse -> session,
                                MoreExecutors.directExecutor()),
                MoreExecutors.directExecutor());
    }

    private static SetSchemaRequest createSetSchemaRequest() throws AppSearchException {
        var builder = new SetSchemaRequest.Builder();
        // Delete old documents incompatible with the new schema.
        builder.setForceOverride(true);
        builder.addDocumentClasses(WebPage.class);
        builder.setDocumentClassDisplayedBySystem(WebPage.class, /* displayed= */ false);

        // Always inlined by R8 - this method is only called by `setUpSessionFuture` if `hooks` is
        // non-null.
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        assumeNonNull(hooks);
        for (PackageIdentifier packageIdentifier : hooks.getPackagesForBrowsingDataVisibility()) {
            builder.setDocumentClassVisibilityForPackage(
                    WebPage.class, /* visible= */ true, packageIdentifier);
        }
        return builder.build();
    }
}
