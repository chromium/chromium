// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwNavigationState;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.support_lib_boundary.WebViewNavigationStateBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_callback_glue.SupportLibWebResourceError;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.Map;

/**
 * Adapter between WebViewNavigationStateBoundaryInterface and AwNavigationState.
 *
 * <p>Once created, instances are kept alive by the peer AwNavigationState.
 */
@Lifetime.Temporary
@NullMarked
class SupportLibWebViewNavigationStateAdapter implements WebViewNavigationStateBoundaryInterface {
    private final AwNavigationState mNavigationState;

    SupportLibWebViewNavigationStateAdapter(AwNavigationState navigationState) {
        mNavigationState = navigationState;
    }

    @Override
    public String getUrl() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_GET_URL")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_URL);
            return mNavigationState.getUrl();
        }
    }

    @Override
    public boolean wasInitiatedByPage() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.NAVIGATION_STATE_WAS_INITIATED_BY_PAGE")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_WAS_INITIATED_BY_PAGE);
            return mNavigationState.wasInitiatedByPage();
        }
    }

    @Override
    public boolean isSameDocument() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_SAME_DOCUMENT")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_SAME_DOCUMENT);
            return mNavigationState.isSameDocument();
        }
    }

    @Override
    public boolean isReload() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_RELOAD")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_RELOAD);
            return mNavigationState.isReload();
        }
    }

    @Override
    public boolean isHistory() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_HISTORY")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_HISTORY);
            return mNavigationState.isHistory();
        }
    }

    @Override
    public boolean isRestore() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_RESTORE")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_RESTORE);
            return mNavigationState.isRestore();
        }
    }

    @Override
    public boolean isBack() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_BACK")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_BACK);
            return mNavigationState.isBack();
        }
    }

    @Override
    public boolean isForward() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_IS_FORWARD")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_IS_FORWARD);
            return mNavigationState.isForward();
        }
    }

    @Override
    public boolean didCommit() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_DID_COMMIT")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_DID_COMMIT);
            return mNavigationState.didCommit();
        }
    }

    @Override
    public boolean didCommitErrorPage() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.NAVIGATION_STATE_DID_COMMIT_ERROR_PAGE")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_DID_COMMIT_ERROR_PAGE);
            return mNavigationState.didCommitErrorPage();
        }
    }

    @Override
    public int getStatusCode() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_GET_STATUS_CODE")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_STATUS_CODE);
            return mNavigationState.getStatusCode();
        }
    }

    @Override
    public long getNavigationStartUptimeMillis() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.NAVIGATION_STATE_GET_NAVIGATION_START_UPTIME_MILLIS")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_NAVIGATION_START_UPTIME_MILLIS);
            return mNavigationState.getNavigationStartUptimeMillis();
        }
    }

    @Override
    public @Nullable /* WebViewPageState */ InvocationHandler getPageState() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_GET_PAGE_STATE")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_PAGE_STATE);
            if (mNavigationState.getPageState() == null) {
                return null;
            }
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewPageStateAdapter(mNavigationState.getPageState()));
        }
    }

    @Override
    public @Nullable /* WebResourceError */ InvocationHandler getWebResourceError() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.NAVIGATION_STATE_GET_WEB_RESOURCE_ERROR")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_WEB_RESOURCE_ERROR);
            if (mNavigationState.getWebResourceError() == null) {
                return null;
            }
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebResourceError(mNavigationState.getWebResourceError()));
        }
    }

    @Override
    public @Nullable Map<String, String> getResponseHeaders() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.NAVIGATION_STATE_GET_RESPONSE_HEADERS")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_RESPONSE_HEADERS);
            return mNavigationState.getResponseHeaders();
        }
    }

    @Override
    public /* WebViewNavigation */ InvocationHandler getNavigation() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.NAVIGATION_STATE_GET_NAVIGATION")) {
            recordApiCall(ApiCall.NAVIGATION_STATE_GET_NAVIGATION);
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewNavigationAdapter(mNavigationState.getNavigation()));
        }
    }
}
