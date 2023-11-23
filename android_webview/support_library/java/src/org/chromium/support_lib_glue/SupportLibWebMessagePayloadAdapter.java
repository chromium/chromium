// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.annotation.SuppressLint;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessagePayloadBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

/** Adapter between WebMessagePayloadBoundaryInterface and MessagePayload in content/. */
@Lifetime.Temporary
class SupportLibWebMessagePayloadAdapter implements WebMessagePayloadBoundaryInterface {
    private final MessagePayload mMessagePayload;

    public SupportLibWebMessagePayloadAdapter(MessagePayload messagePayload) {
        mMessagePayload = messagePayload;
    }

    @Override
    public String[] getSupportedFeatures() {
        // getType() and getAsString() are already covered by WEB_MESSAGE_GET_MESSAGE_PAYLOAD.
        return new String[0];
    }

    @SuppressLint("WrongConstant")
    @Override
    public int getType() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_PAYLOAD_GET_TYPE")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PAYLOAD_GET_TYPE);
            return mMessagePayload.getType();
        }
    }

    @Nullable
    @Override
    public String getAsString() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_MESSAGE_PAYLOAD_GET_AS_STRING")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_STRING);
            return mMessagePayload.getAsString();
        }
    }

    @NonNull
    @Override
    public byte[] getAsArrayBuffer() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER")) {
            recordApiCall(ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER);
            return mMessagePayload.getAsArrayBuffer();
        }
    }

    public /* MessagePayload */ InvocationHandler getInvocationHandler() {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(this);
    }

    public static MessagePayload fromWebMessageBoundaryInterface(
            @NonNull WebMessageBoundaryInterface boundaryInterface) {
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                boundaryInterface.getSupportedFeatures(), Features.WEB_MESSAGE_ARRAY_BUFFER)) {
            // MessagePayload API is supported by AndroidX.
            final MessagePayload messagePayload =
                    SupportLibWebMessagePayloadAdapter.toMessagePayload(
                            boundaryInterface.getMessagePayload());
            if (messagePayload != null) {
                // MessagePayload API is not supported by app.
                return messagePayload;
            }
        }
        // Fallback to old string-only API.
        return new MessagePayload(boundaryInterface.getData());
    }

    public static MessagePayload toMessagePayload(
            /* MessagePayload */ InvocationHandler invocationHandler) {
        if (invocationHandler == null) {
            return null;
        }
        WebMessagePayloadBoundaryInterface webMessagePayloadBoundaryInterface =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebMessagePayloadBoundaryInterface.class, invocationHandler);
        @WebMessagePayloadType final int type = webMessagePayloadBoundaryInterface.getType();
        switch (type) {
            case WebMessagePayloadType.TYPE_STRING:
                return new MessagePayload(webMessagePayloadBoundaryInterface.getAsString());
            case WebMessagePayloadType.TYPE_ARRAY_BUFFER:
                return new MessagePayload(webMessagePayloadBoundaryInterface.getAsArrayBuffer());
            default:
                // String and ArrayBuffer are covered by WEB_MESSAGE_GET_MESSAGE_PAYLOAD feature.
                // Please add new feature flags for new types.
                throw new IllegalArgumentException("Unsupported type: " + type);
        }
    }
}
