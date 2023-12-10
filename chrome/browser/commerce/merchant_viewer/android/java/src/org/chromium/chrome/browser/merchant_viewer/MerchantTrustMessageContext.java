// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

/** Additional data required for publishing and handling a merchant trust signals message. */
class MerchantTrustMessageContext {
    private final WebContents mWebContents;
    private final NavigationHandle mNavigationHandle;

    /** Creates a new instance. */
    MerchantTrustMessageContext(
            @NonNull NavigationHandle navigationHandle, @NonNull WebContents webContents) {
        mNavigationHandle = navigationHandle;
        mWebContents = webContents;
    }

    /**
     * Returns the host name for which the message is intended to be shown. Can return null if the
     * navigationHandle or its GURL is null.
     */
    @Nullable
    String getHostName() {
        return (mNavigationHandle == null || mNavigationHandle.getUrl() == null)
                ? null
                : mNavigationHandle.getUrl().getHost();
    }

    /**
     * Returns the URL for which the message is intended to be shown. Can return null if the
     * navigationHandle or its GURL is null.
     */
    @Nullable
    String getUrl() {
        return (mNavigationHandle == null || mNavigationHandle.getUrl() == null)
                ? null
                : mNavigationHandle.getUrl().getSpec();
    }

    /* Returns the {@link WebContentns} for which the message is intended to be shown. */
    WebContents getWebContents() {
        return mWebContents;
    }

    /** Returns the {@link NavigationHandle} associated with the context. */
    NavigationHandle getNavigationHandle() {
        return mNavigationHandle;
    }

    /* Checks whether or not the context is valid. */
    boolean isValid() {
        return mWebContents != null
                && !mWebContents.isDestroyed()
                && mNavigationHandle != null
                && mNavigationHandle.getUrl() != null
                && !mNavigationHandle.getUrl().isEmpty();
    }
}
