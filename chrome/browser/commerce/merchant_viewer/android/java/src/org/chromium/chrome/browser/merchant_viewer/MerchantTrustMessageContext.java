// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

/** Additional data required for publishing and handling a merchant trust signals message. */
@NullMarked
class MerchantTrustMessageContext {
    private final @Nullable WebContents mWebContents;
    private final NavigationHandle mNavigationHandle;

    /** Creates a new instance. */
    MerchantTrustMessageContext(
            NavigationHandle navigationHandle, @Nullable WebContents webContents) {
        mNavigationHandle = navigationHandle;
        mWebContents = webContents;
    }

    /**
     * Returns the host name for which the message is intended to be shown. Can return null if the
     * navigationHandle or its GURL is null.
     */
    String getHostName() {
        return (mNavigationHandle == null || mNavigationHandle.getUrl() == null)
                ? ""
                : mNavigationHandle.getUrl().getHost();
    }

    /**
     * Returns the URL for which the message is intended to be shown. Can return null if the
     * navigationHandle or its GURL is null.
     */
    @Nullable String getUrl() {
        return (mNavigationHandle == null || mNavigationHandle.getUrl() == null)
                ? null
                : mNavigationHandle.getUrl().getSpec();
    }

    /** Returns the {@link WebContents} for which the message is intended to be shown. */
    @Nullable WebContents getWebContents() {
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
