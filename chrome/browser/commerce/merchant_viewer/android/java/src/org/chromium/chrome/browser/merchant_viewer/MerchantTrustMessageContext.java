// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.text.TextUtils;

import org.chromium.content_public.browser.WebContents;

/**
 * Additional data required for publishing and handling a merchant trust signals message.
 */
class MerchantTrustMessageContext {
    private final String mHostName;
    private final WebContents mWebContents;

    /** Creates a new instance. */
    MerchantTrustMessageContext(String hostName, WebContents webContents) {
        mHostName = hostName;
        mWebContents = webContents;
    }

    /** Returns the host name for which the message is intended to be shown. */
    String getHostName() {
        return mHostName;
    }

    /* Returns the {@link WebContentns} for which the message is intended to be shown. */
    WebContents getWebContents() {
        return mWebContents;
    }

    /* Checks whether or not the context is valid. */
    boolean isValid() {
        return mWebContents != null && !mWebContents.isDestroyed() && !TextUtils.isEmpty(mHostName);
    }
}