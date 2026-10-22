// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.GoogleAccountsEvent;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.support_lib_boundary.GoogleAccountsEventBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/** Adapter between GoogleAccountsEventBoundaryInterface and GoogleAccountsEvent. */
@Lifetime.Temporary
@NullMarked
public class SupportLibGoogleAccountsEventAdapter implements GoogleAccountsEventBoundaryInterface {
    private final GoogleAccountsEvent mEvent;

    public SupportLibGoogleAccountsEventAdapter(GoogleAccountsEvent event) {
        mEvent = event;
    }

    @Override
    public @ActionType int getActionType() {
        try (TraceEvent traceEvent =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.GOOGLE_ACCOUNTS_EVENT_GET_ACTION_TYPE")) {
            recordApiCall(ApiCall.GOOGLE_ACCOUNTS_EVENT_GET_ACTION_TYPE);
            switch (mEvent.getServiceType()) {
                case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNOUT:
                    return ACTION_TYPE_SIGN_OUT;
                case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP:
                    return ACTION_TYPE_CREATE_ACCOUNT;
                default:
                    return ACTION_TYPE_OTHER;
            }
        }
    }

    @Override
    public String getContinueUrl() {
        try (TraceEvent traceEvent =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.GOOGLE_ACCOUNTS_EVENT_GET_CONTINUE_URL")) {
            recordApiCall(ApiCall.GOOGLE_ACCOUNTS_EVENT_GET_CONTINUE_URL);
            return mEvent.getContinueUrl();
        }
    }

    @Override
    public boolean isOutermostMainFrame() {
        try (TraceEvent traceEvent =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.GOOGLE_ACCOUNTS_EVENT_IS_OUTERMOST_MAIN_FRAME")) {
            recordApiCall(ApiCall.GOOGLE_ACCOUNTS_EVENT_IS_OUTERMOST_MAIN_FRAME);
            return mEvent.isOutermostMainFrame();
        }
    }

    @Override
    public boolean isNavigationRequest() {
        try (TraceEvent traceEvent =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.GOOGLE_ACCOUNTS_EVENT_IS_NAVIGATION_REQUEST")) {
            recordApiCall(ApiCall.GOOGLE_ACCOUNTS_EVENT_IS_NAVIGATION_REQUEST);
            return mEvent.isNavigationRequest();
        }
    }
}
