// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Handler;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;

import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

/**
 * This class is used to convert a WebMessagePort to a MessagePort in chromium
 * world.
 */
public class WebMessagePortAdapter extends WebMessagePort {
    private MessagePort mPort;

    public WebMessagePortAdapter(MessagePort port) {
        mPort = port;
    }

    @Override
    public void postMessage(WebMessage message) {
        mPort.postMessage(
                new MessagePayload(message.getData()), toMessagePorts(message.getPorts()));
    }

    @Override
    public void close() {
        mPort.close();
    }

    @Override
    public void setWebMessageCallback(WebMessageCallback callback) {
        setWebMessageCallback(callback, null);
    }

    @Override
    public void setWebMessageCallback(final WebMessageCallback callback, final Handler handler) {
        mPort.setMessageCallback(
                new MessagePort.MessageCallback() {
                    @Override
                    public void onMessage(MessagePayload messagePayload, MessagePort[] ports) {
                        callback.onMessage(
                                WebMessagePortAdapter.this,
                                new WebMessage(
                                        messagePayload.getAsString(), fromMessagePorts(ports)));
                    }
                },
                handler);
    }

    public MessagePort getPort() {
        return mPort;
    }

    public static WebMessagePort[] fromMessagePorts(MessagePort[] messagePorts) {
        if (messagePorts == null) return null;
        WebMessagePort[] ports = new WebMessagePort[messagePorts.length];
        for (int i = 0; i < messagePorts.length; i++) {
            ports[i] = new WebMessagePortAdapter(messagePorts[i]);
        }
        return ports;
    }

    public static MessagePort[] toMessagePorts(WebMessagePort[] webMessagePorts) {
        if (webMessagePorts == null) return null;
        MessagePort[] ports = new MessagePort[webMessagePorts.length];
        for (int i = 0; i < webMessagePorts.length; i++) {
            ports[i] = ((WebMessagePortAdapter) webMessagePorts[i]).getPort();
        }
        return ports;
    }
}
