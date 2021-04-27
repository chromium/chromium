// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.NonNull;

import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
/**
 * Additional data required for publishing and handling a merchant trust signals message.
 */
class MerchantTrustMessageContext {
    private final WebContents mWebContents;
    private final GURL mUrl;

    /** Creates a new instance. */
    MerchantTrustMessageContext(@NonNull GURL url, @NonNull WebContents webContents) {
        mUrl = url;
        mWebContents = webContents;
    }

    /** Returns the host name for which the message is intended to be shown. */
    String getHostName() {
        return mUrl == null ? "" : mUrl.getHost();
    }

    /* Returns the {@link WebContentns} for which the message is intended to be shown. */
    WebContents getWebContents() {
        return mWebContents;
    }

    /** Returns the {@link GURL} associated with the context. */
    GURL getUrl() {
        return mUrl;
    }

    /* Checks whether or not the context is valid. */
    boolean isValid() {
        return mWebContents != null && !mWebContents.isDestroyed() && mUrl != null
                && !mUrl.isEmpty();
    }
}