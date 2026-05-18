// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.VisibleForTesting;
import androidx.appsearch.builtintypes.WebPage;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Java bridge to allow C++'s `AuxiliarySearchDonationService` to donate browsing history to other
 * apps via AppSearch.
 */
@NullMarked
class AuxiliarySearchDonationServiceBridge {
    // Differs from `AuxiliarySearchDonor`, which uses the package name as the namespace.
    @VisibleForTesting static final String HISTORY_NAMESPACE = "History";
    @VisibleForTesting static final long HISTORY_DOCUMENT_TTL_MILLIS = TimeUnit.HOURS.toMillis(24);

    @CalledByNative
    public AuxiliarySearchDonationServiceBridge() {}

    @CalledByNative
    public void donateHistory(
            @JniType("std::vector<AuxiliarySearchDonationService::HistoryData>")
                    List<WebPage> pages) {
        // TODO: crbug.com/432359106 - Implement the donation.
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
}
