// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.content_public.browser.NavigationHandle;

/** Represents a navigation and is exposed to embedders. See also AwNavigationClient */
public class AwNavigation extends AwSupportLibIsomorphic {
    private final NavigationHandle mNavigationHandle;

    public AwNavigation(NavigationHandle navigationHandle) {
        mNavigationHandle = navigationHandle;
    }

    // TODO(crbug.com/394479273): Add Page-related functions.

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
        // TODO: Expose this info from NavigationHandle.
        return false;
    }

    public boolean isRestore() {
        // TODO: Expose this info from NavigationHandle.
        return false;
    }

    public boolean isBack() {
        // TODO: Expose this info from NavigationHandle.
        return false;
    }

    public boolean isForward() {
        // TODO: Expose this info from NavigationHandle.
        return false;
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
