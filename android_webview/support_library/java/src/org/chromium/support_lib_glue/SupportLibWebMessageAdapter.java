// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.base.TraceEvent;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

/**
 * Utility class for creating a WebMessageBoundaryInterface (this is necessary to pass a
 * WebMessage back across the boundary).
 */
public class SupportLibWebMessageAdapter implements WebMessageBoundaryInterface {
    private MessagePayload mMessagePayload;
    private MessagePort[] mPorts;

    /* package */ SupportLibWebMessageAdapter(MessagePayload messagePayload, MessagePort[] ports) {
        mMessagePayload = messagePayload;
        mPorts = ports;
    }

    @Override
    public String getData() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_GET_DATA")) {
            recordApiCall(ApiCall.WEB_MESSAGE_GET_DATA);
            return mMessagePayload.getAsString();
        }
    }

    @Override
    public /* MessagePayload */ InvocationHandler getMessagePayload() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_GET_MESSAGE_PAYLOAD")) {
            recordApiCall(ApiCall.WEB_MESSAGE_GET_MESSAGE_PAYLOAD);
            return new SupportLibWebMessagePayloadAdapter(mMessagePayload).getInvocationHandler();
        }
    }

    @Override
    public /* WebMessagePort */ InvocationHandler[] getPorts() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_GET_PORTS")) {
            recordApiCall(ApiCall.WEB_MESSAGE_GET_PORTS);
            return SupportLibWebMessagePortAdapter.fromMessagePorts(mPorts);
        }
    }

    @Override
    public String[] getSupportedFeatures() {
        // getData() and getPorts() are not covered by feature flags.
        return new String[] {Features.WEB_MESSAGE_ARRAY_BUFFER};
    }
}
