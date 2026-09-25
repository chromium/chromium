// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;

import java.util.Map;
import java.util.function.BiFunction;

/** Represents a navigation and is exposed to embedders. See also AwNavigationListener */
@NullMarked
public class AwNavigation extends AwSupportLibIsomorphic {
    private final NavigationHandle mNavigationHandle;
    private final BiFunction<NavigationHandle, AwNavigation, AwNavigationState>
            mNavigationStateProvider;
    // The Page that the navigation commits into. Set to null if the navigation doesn't commit or
    // result in a Page (e.g. 204/download)
    private @Nullable AwPage mPage;
    private @Nullable Map<String, String> mResponseHeaders;

    public AwNavigation(
            NavigationHandle navigationHandle,
            @Nullable AwPage page,
            BiFunction<NavigationHandle, AwNavigation, AwNavigationState> navigationStateProvider) {
        mNavigationHandle = navigationHandle;
        mPage = page;
        mNavigationStateProvider = navigationStateProvider;
    }

    // Deprecated method, see AwNavigationState instead
    void setPage(@Nullable AwPage page) {
        if (mPage != page) {
            // We can only change the page associated with the navigation if it was null before
            // (e.g. the AwNavigation was constructed when the navigation just started, then
            // the navigation eventually committed a page).
            assert mPage == null;
        }
        mPage = page;
    }

    // Deprecated method, see AwNavigationState instead
    public @Nullable AwPage getPage() {
        return mPage;
    }

    // Deprecated method, see AwNavigationState instead
    public String getUrl() {
        return mNavigationHandle.getUrl().getValidSpecOrEmpty();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean wasInitiatedByPage() {
        return mNavigationHandle.isRendererInitiated();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isSameDocument() {
        return mNavigationHandle.isSameDocument();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isReload() {
        return mNavigationHandle.isReload();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isHistory() {
        return mNavigationHandle.isHistory();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isRestore() {
        return mNavigationHandle.isRestore();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isBack() {
        return mNavigationHandle.isBack();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean isForward() {
        return mNavigationHandle.isForward();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean didCommit() {
        return mNavigationHandle.hasCommitted();
    }

    // Deprecated method, see AwNavigationState instead
    public boolean didCommitErrorPage() {
        return mNavigationHandle.isErrorPage();
    }

    // Deprecated method, see AwNavigationState instead
    public int getStatusCode() {
        return mNavigationHandle.httpStatusCode();
    }

    // Deprecated method, see AwNavigationState instead
    public long getNavigationStartUptimeMillis() {
        return mNavigationHandle.getNavigationStartMs();
    }

    // Deprecated method, see AwNavigationState instead
    public @Nullable AwWebResourceError getWebResourceError() {
        if (mNavigationHandle.errorCode() == NetError.OK) return null;
        return AwWebResourceError.createFromNetError(
                mNavigationHandle.errorCode(), mNavigationHandle.errorDescription());
    }

    // Deprecated method, see AwNavigationState instead
    public @Nullable Map<String, String> getResponseHeaders() {
        if (mResponseHeaders == null) {
            mResponseHeaders = mNavigationHandle.getResponseHeaders();
        }
        return mResponseHeaders;
    }

    @AnyThread
    public AwNavigationState snapshotState() {
        return mNavigationStateProvider.apply(mNavigationHandle, this);
    }
}
