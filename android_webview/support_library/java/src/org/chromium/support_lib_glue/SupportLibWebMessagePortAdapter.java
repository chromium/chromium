// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.os.Handler;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessageCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessagePortBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

/** Adapter between WebMessagePortBoundaryInterface and MessagePort. */
@Lifetime.Temporary
class SupportLibWebMessagePortAdapter implements WebMessagePortBoundaryInterface {
    private MessagePort mPort;

    SupportLibWebMessagePortAdapter(MessagePort port) {
        mPort = port;
    }

    /* package */ MessagePort getPort() {
        return mPort;
    }

    @Override
    public void postMessage(InvocationHandler message) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_PORT_POST_MESSAGE")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PORT_POST_MESSAGE);
            WebMessageBoundaryInterface messageBoundaryInterface =
                    BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                            WebMessageBoundaryInterface.class, message);
            mPort.postMessage(
                    SupportLibWebMessagePayloadAdapter.fromWebMessageBoundaryInterface(
                            messageBoundaryInterface),
                    toMessagePorts(messageBoundaryInterface.getPorts()));
        }
    }

    @Override
    public void close() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_PORT_CLOSE")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PORT_CLOSE);
            mPort.close();
        }
    }

    @Override
    public void setWebMessageCallback(InvocationHandler callback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_PORT_SET_CALLBACK")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK);
            setWebMessageCallbackInternal(callback, null);
        }
    }

    @Override
    public void setWebMessageCallback(InvocationHandler callback, Handler handler) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER);
            setWebMessageCallbackInternal(callback, handler);
        }
    }

    private void setWebMessageCallbackInternal(
            /* WebMessageCallback */ InvocationHandler callback, Handler handler) {
        SupportLibWebMessageCallbackAdapter callbackAdapter =
                new SupportLibWebMessageCallbackAdapter(
                        BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                                WebMessageCallbackBoundaryInterface.class, callback));
        mPort.setMessageCallback(
                new MessagePort.MessageCallback() {
                    @Override
                    public void onMessage(MessagePayload messagePayload, MessagePort[] ports) {
                        callbackAdapter.onMessage(
                                SupportLibWebMessagePortAdapter.this,
                                new SupportLibWebMessageAdapter(messagePayload, ports));
                    }
                },
                handler);
    }

    public static /* WebMessagePort */ InvocationHandler[] fromMessagePorts(
            MessagePort[] messagePorts) {
        if (messagePorts == null) return null;

        SupportLibWebMessagePortAdapter[] ports =
                new SupportLibWebMessagePortAdapter[messagePorts.length];
        for (int i = 0; i < messagePorts.length; i++) {
            ports[i] = new SupportLibWebMessagePortAdapter(messagePorts[i]);
        }
        return BoundaryInterfaceReflectionUtil.createInvocationHandlersForArray(ports);
    }

    public static MessagePort[] toMessagePorts(InvocationHandler[] webMessagePorts) {
        if (webMessagePorts == null) return null;

        MessagePort[] ports = new MessagePort[webMessagePorts.length];
        for (int i = 0; i < webMessagePorts.length; i++) {
            // We are here assuming that the support library side is passing an array of
            // InvocationHandlers that were created in WebView APK code through calls to
            // BoundaryInterfaceReflectionUtil.createInvocationHandlerFor. I.e. we are assuming that
            // the support library side is not creating new InvocationHandlers for those
            // WebMessagePorts, but instead passing back the original InvocationHandlers.
            SupportLibWebMessagePortAdapter messagePortAdapter =
                    (SupportLibWebMessagePortAdapter)
                            BoundaryInterfaceReflectionUtil.getDelegateFromInvocationHandler(
                                    webMessagePorts[i]);
            ports[i] = messagePortAdapter.mPort;
        }
        return ports;
    }
}
