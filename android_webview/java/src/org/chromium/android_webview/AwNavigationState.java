// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationState;
import org.chromium.net.NetError;

import java.util.Map;

/** Represents a NavigationState and is exposed to embedders. See also AwNavigationListener */
@AnyThread
@NullMarked
public class AwNavigationState {

    private final NavigationState mNavigationState;
    private final AwNavigation mNavigation;
    // The Page that the navigation commits into. Set to null if the navigation doesn't commit or
    // result in a Page (e.g. 204/download)
    private final @Nullable AwPageState mPageState;

    public AwNavigationState(
            NavigationState navigationState,
            AwNavigation navigation,
            @Nullable AwPageState pageState) {
        mNavigationState = navigationState;
        mNavigation = navigation;
        mPageState = pageState;
    }

    public @Nullable AwPageState getPageState() {
        return mPageState;
    }

    public String getUrl() {
        return mNavigationState.getUrl().getValidSpecOrEmpty();
    }

    public boolean wasInitiatedByPage() {
        return mNavigationState.isRendererInitiated();
    }

    public boolean isSameDocument() {
        return mNavigationState.isSameDocument();
    }

    public boolean isReload() {
        return mNavigationState.isReload();
    }

    public boolean isHistory() {
        return mNavigationState.isHistory();
    }

    public boolean isRestore() {
        return mNavigationState.isRestore();
    }

    public boolean isBack() {
        return mNavigationState.isBack();
    }

    public boolean isForward() {
        return mNavigationState.isForward();
    }

    public boolean didCommit() {
        return mNavigationState.hasCommitted();
    }

    public boolean didCommitErrorPage() {
        return mNavigationState.isErrorPage();
    }

    public int getStatusCode() {
        return mNavigationState.httpStatusCode();
    }

    public long getNavigationStartUptimeMillis() {
        return mNavigationState.getNavigationStartMs();
    }

    public @Nullable AwWebResourceError getWebResourceError() {
        if (mNavigationState.errorCode() == NetError.OK) return null;
        return AwWebResourceError.createFromNetError(
                mNavigationState.errorCode(), mNavigationState.errorDescription());
    }

    public @Nullable Map<String, String> getResponseHeaders() {
        return mNavigationState.getResponseHeaders();
    }

    public AwNavigation getNavigation() {
        return mNavigation;
    }
}
