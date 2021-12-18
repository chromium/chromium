// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * A Java API for connecting to shared_highlighting component.
 */
public class LinkToTextBridge {
    // TODO(gayane): Update the name whenever |shared_highlighting::ShouldOfferLinkToText| updated
    // to more descriptive name.
    public static boolean shouldOfferLinkToText(GURL url) {
        return LinkToTextBridgeJni.get().shouldOfferLinkToText(url);
    }

    public static void logFailureMetrics(@LinkGenerationError int error) {
        LinkToTextBridgeJni.get().logFailureMetrics(error);
    }

    public static void logSuccessMetrics() {
        LinkToTextBridgeJni.get().logSuccessMetrics();
    }

    public static void logLinkRequestedBeforeStatus(
            @LinkGenerationStatus int status, @LinkGenerationReadyStatus int readyStatus) {
        LinkToTextBridgeJni.get().logLinkRequestedBeforeStatus(status, readyStatus);
    }

    @NativeMethods
    interface Natives {
        boolean shouldOfferLinkToText(GURL url);
        void logFailureMetrics(@LinkGenerationError int error);
        void logSuccessMetrics();
        void logLinkRequestedBeforeStatus(
                @LinkGenerationStatus int status, @LinkGenerationReadyStatus int readyStatus);
    }
}
