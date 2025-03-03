// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHandle;

/** Represents a navigation and is exposed to embedders. See also AwNavigationClient */
public class AwNavigation extends AwSupportLibIsomorphic {
    private final NavigationHandle mNavigationHandle;
    // The Page that the navigation commits into. Set to null if the navigation doesn't commit or
    // result in a Page (e.g. 204/download)
    private final @Nullable AwPage mPage;

    public AwNavigation(NavigationHandle navigationHandle, @Nullable AwPage page) {
        mNavigationHandle = navigationHandle;
        mPage = page;
    }

    public @Nullable AwPage getPage() {
        return mPage;
    }

    public String getUrl() {
        return mNavigationHandle.getUrl().getValidSpecOrEmpty();
    }

    public boolean isPageInitiated() {
        return mNavigationHandle.isRendererInitiated();
    }

    public boolean isSameDocument() {
        return mNavigationHandle.isSameDocument();
    }

    public boolean isReload() {
        return mNavigationHandle.isReload();
    }

    public boolean isHistory() {
        return mNavigationHandle.isHistory();
    }

    public boolean isRestore() {
        return mNavigationHandle.isRestore();
    }

    public boolean isBack() {
        return mNavigationHandle.isBack();
    }

    public boolean isForward() {
        return mNavigationHandle.isForward();
    }

    public boolean hasCommitted() {
        return mNavigationHandle.hasCommitted();
    }

    public boolean didCommitErrorPage() {
        return mNavigationHandle.isErrorPage();
    }

    public int getStatusCode() {
        return mNavigationHandle.httpStatusCode();
    }
}
