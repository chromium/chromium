// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_image_service;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.page_image_service.mojom.ClientId.EnumType;

/** Allows java access to the native ImageService. */
public class ImageServiceMetrics {
    static final String HISTOGRAM_SALIENT_IMAGE_URL_FETCH_RESULT_PREFIX =
            "PageImageService.Android.SalientImageUrlFetchResult.";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    //
    // The values must be consistent with SalientImageUrlFetchResult in enums.xml.
    @IntDef({
        SalientImageUrlFetchResult.FAILED_FROM_NETWORK,
        SalientImageUrlFetchResult.FAILED_FROM_CACHE,
        SalientImageUrlFetchResult.SUCCEED_FROM_NETWORK,
        SalientImageUrlFetchResult.SUCCEED_FROM_CACHE,
        SalientImageUrlFetchResult.NUM_ENTRIES
    })
    @interface SalientImageUrlFetchResult {
        int FAILED_FROM_NETWORK = 0;
        int FAILED_FROM_CACHE = 1;
        int SUCCEED_FROM_NETWORK = 2;
        int SUCCEED_FROM_CACHE = 3;
        int NUM_ENTRIES = 4;
    }

    public static void recordFetchImageUrlResult(
            @EnumType int clientId, @SalientImageUrlFetchResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SALIENT_IMAGE_URL_FETCH_RESULT_PREFIX
                        + ImageServiceBridge.clientIdToString(clientId),
                result,
                SalientImageUrlFetchResult.NUM_ENTRIES);
    }
}
