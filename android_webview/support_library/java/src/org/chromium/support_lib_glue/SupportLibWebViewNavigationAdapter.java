// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.WebViewNavigationBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

/**
 * Adapter between WebViewNavigationBoundaryInterface and AwNavigation.
 *
 * <p>Once created, instances are kept alive by the peer AwNavigation.
 */
@Lifetime.Temporary
class SupportLibWebViewNavigationAdapter extends IsomorphicAdapter
        implements WebViewNavigationBoundaryInterface {
    private final AwNavigation mNavigation;

    SupportLibWebViewNavigationAdapter(AwNavigation navigation) {
        mNavigation = navigation;
    }

    @Override
    AwSupportLibIsomorphic getPeeredObject() {
        return mNavigation;
    }

    @Override
    public String getUrl() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_GET_URL")) {
            recordApiCall(ApiCall.NAVIGATION_GET_URL);
            return mNavigation.getUrl();
        }
    }

    @Override
    public boolean wasInitiatedByPage() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_WAS_INITIATED_BY_PAGE")) {
            recordApiCall(ApiCall.NAVIGATION_WAS_INITIATED_BY_PAGE);
            return mNavigation.wasInitiatedByPage();
        }
    }

    @Override
    public boolean isSameDocument() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_SAME_DOCUMENT")) {
            recordApiCall(ApiCall.NAVIGATION_IS_SAME_DOCUMENT);
            return mNavigation.isSameDocument();
        }
    }

    @Override
    public boolean isReload() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_RELOAD")) {
            recordApiCall(ApiCall.NAVIGATION_IS_RELOAD);
            return mNavigation.isReload();
        }
    }

    @Override
    public boolean isHistory() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_HISTORY")) {
            recordApiCall(ApiCall.NAVIGATION_IS_HISTORY);
            return mNavigation.isHistory();
        }
    }

    @Override
    public boolean isRestore() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_RESTORE")) {
            recordApiCall(ApiCall.NAVIGATION_IS_RESTORE);
            return mNavigation.isRestore();
        }
    }

    @Override
    public boolean isBack() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_BACK")) {
            recordApiCall(ApiCall.NAVIGATION_IS_BACK);
            return mNavigation.isBack();
        }
    }

    @Override
    public boolean isForward() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_IS_FORWARD")) {
            recordApiCall(ApiCall.NAVIGATION_IS_FORWARD);
            return mNavigation.isForward();
        }
    }

    @Override
    public boolean didCommit() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_DID_COMMIT")) {
            recordApiCall(ApiCall.NAVIGATION_DID_COMMIT);
            return mNavigation.didCommit();
        }
    }

    @Override
    public boolean didCommitErrorPage() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_DID_COMMIT_ERROR_PAGE")) {
            recordApiCall(ApiCall.NAVIGATION_DID_COMMIT_ERROR_PAGE);
            return mNavigation.didCommitErrorPage();
        }
    }

    @Override
    public int getStatusCode() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_GET_STATUS_CODE")) {
            recordApiCall(ApiCall.NAVIGATION_GET_STATUS_CODE);
            return mNavigation.getStatusCode();
        }
    }

    @Override
    public /* WebViewPage */ @Nullable InvocationHandler getPage() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_GET_PAGE")) {
            recordApiCall(ApiCall.NAVIGATION_GET_PAGE);
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewPageAdapter(mNavigation.getPage()));
        }
    }
}
