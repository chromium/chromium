// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.JavaScriptExecutionCallback;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.base.TraceEvent;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.js_injection.mojom.JavaScriptExecutionError;
import org.chromium.support_lib_boundary.ExecuteJavaScriptCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.ExecuteJavaScriptCallbackBoundaryInterface.ExecuteJavaScriptExceptionTypeBoundaryInterface;
import org.chromium.support_lib_boundary.JsReplyProxyBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Callable;

/** Adapter between JsReplyProxyBoundaryInterface and JsReplyProxy. */
class SupportLibJsReplyProxyAdapter implements JsReplyProxyBoundaryInterface {
    private final JsReplyProxy mReplyProxy;

    public SupportLibJsReplyProxyAdapter(JsReplyProxy replyProxy) {
        mReplyProxy = replyProxy;
    }

    @Override
    public void postMessage(String message) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.JS_REPLY_POST_MESSAGE")) {
            recordApiCall(ApiCall.JS_REPLY_POST_MESSAGE);
            mReplyProxy.postMessage(new MessagePayload(message));
        }
    }

    @Override
    public void postMessageWithPayload(/* MessagePayload */ InvocationHandler payload) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.JS_REPLY_POST_MESSAGE_WITH_PAYLOAD")) {
            recordApiCall(ApiCall.JS_REPLY_POST_MESSAGE_WITH_PAYLOAD);
            mReplyProxy.postMessage(SupportLibWebMessagePayloadAdapter.toMessagePayload(payload));
        }
    }

    @Override
    public void executeJavaScript(
            String javaScript, /* ExecuteJavaScriptCallbackBoundaryInterface */
            InvocationHandler callback) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.JS_REPLY_EXECUTE_JS")) {
            recordApiCall(ApiCall.JS_REPLY_EXECUTE_JS);
            mReplyProxy.executeJavaScript(javaScript, createExecutionCallback(callback));
        }
    }

    @Override
    public Object getOrCreatePeer(Callable<Object> creationCallable) {
        return mReplyProxy.getOrCreateSupportLibObject(creationCallable);
    }

    private JavaScriptExecutionCallback createExecutionCallback(
            /* ExecuteJavaScriptCallbackBoundaryInterface */ InvocationHandler callback) {
        ExecuteJavaScriptCallbackBoundaryInterface executionCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        ExecuteJavaScriptCallbackBoundaryInterface.class, callback);
        return new JavaScriptExecutionCallback() {
            @Override
            public void onSuccess(String result) {
                executionCallback.onSuccess(result);
            }

            @Override
            public void onError(@JavaScriptExecutionError.EnumType int errorCode) {
                mapFailure(executionCallback, errorCode);
            }
        };
    }

    private void mapFailure(
            ExecuteJavaScriptCallbackBoundaryInterface callback,
            @JavaScriptExecutionError.EnumType int errorCode) {
        int type =
                switch (errorCode) {
                    case JavaScriptExecutionError.FRAME_DESTROYED ->
                            ExecuteJavaScriptExceptionTypeBoundaryInterface.FRAME_DESTROYED;
                    default -> ExecuteJavaScriptExceptionTypeBoundaryInterface.GENERIC;
                };
        String message =
                switch (errorCode) {
                    case JavaScriptExecutionError.FRAME_DESTROYED ->
                            null; // No message for this error
                    default -> "Unknown error occurred.";
                };
        callback.onFailure(type, message);
    }
}
