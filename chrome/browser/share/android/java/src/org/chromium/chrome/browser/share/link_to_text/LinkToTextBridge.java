// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** A Java API for connecting to shared_highlighting component. */
public class LinkToTextBridge {
    // TODO(gayane): Update the name whenever |shared_highlighting::ShouldOfferLinkToText| updated
    // to more descriptive name.
    public static boolean shouldOfferLinkToText(GURL url) {
        return LinkToTextBridgeJni.get().shouldOfferLinkToText(url);
    }

    public static boolean supportsLinkGenerationInIframe(GURL url) {
        return LinkToTextBridgeJni.get().supportsLinkGenerationInIframe(url);
    }

    public static void logFailureMetrics(WebContents webContents, @LinkGenerationError int error) {
        LinkToTextBridgeJni.get().logFailureMetrics(webContents, error);
    }

    public static void logSuccessMetrics(WebContents webContents) {
        LinkToTextBridgeJni.get().logSuccessMetrics(webContents);
    }

    public static void logLinkRequestedBeforeStatus(
            @LinkGenerationStatus int status, @LinkGenerationReadyStatus int readyStatus) {
        LinkToTextBridgeJni.get().logLinkRequestedBeforeStatus(status, readyStatus);
    }

    public static void logLinkToTextReshareStatus(@LinkToTextReshareStatus int status) {
        LinkToTextBridgeJni.get().logLinkToTextReshareStatus(status);
    }

    @NativeMethods
    interface Natives {
        boolean shouldOfferLinkToText(GURL url);

        boolean supportsLinkGenerationInIframe(GURL url);

        void logFailureMetrics(WebContents webContents, @LinkGenerationError int error);

        void logSuccessMetrics(WebContents webContents);

        void logLinkRequestedBeforeStatus(
                @LinkGenerationStatus int status, @LinkGenerationReadyStatus int readyStatus);

        void logLinkToTextReshareStatus(@LinkToTextReshareStatus int status);
    }
}
