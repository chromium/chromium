// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.ScriptHandler;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.ScriptHandlerBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/** Adapter between ScriptHandlerBoundaryInterface and ScriptHandler. */
interface SupportLibScriptHandlerAdapter extends ScriptHandlerBoundaryInterface {

    /**
     * Creates a SupportLibScriptHandlerAdapter that logs it's removing a script added by
     * ADD_DOCUMENT_START. Note: Logging and tracing API calls is the only difference between this
     * and {@code persistentJavascriptHandler}.
     */
    public static SupportLibScriptHandlerAdapter documentStartHandler(
            final ScriptHandler scriptHandler) {
        return () -> {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.REMOVE_DOCUMENT_START_SCRIPT")) {
                recordApiCall(ApiCall.REMOVE_DOCUMENT_START_SCRIPT);
                scriptHandler.remove();
            }
        };
    }

    /**
     * Creates a SupportLibScriptHandlerAdapter that logs it's removing a script added by
     * ADD_JAVA_SCRIPT_ON_EVENT. Note: Logging and tracing API calls is the only difference between
     * this and {@code documentStartHandler}.
     */
    public static SupportLibScriptHandlerAdapter persistentJavascriptHandler(
            final ScriptHandler scriptHandler) {
        return () -> {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.REMOVE_JAVA_SCRIPT_ON_EVENT")) {
                recordApiCall(ApiCall.REMOVE_JAVA_SCRIPT_ON_EVENT);
                scriptHandler.remove();
            }
        };
    }
}
