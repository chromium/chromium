// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.annotation.SuppressLint;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.MessagePayload;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessagePayloadBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;

/**
 * Adapter between WebMessagePayloadBoundaryInterface and MessagePayload in content/.
 */
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
        return mMessagePayload.getType();
    }

    @Nullable
    @Override
    public String getAsString() {
        return mMessagePayload.getAsString();
    }

    public /* MessagePayload */ InvocationHandler getInvocationHandler() {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(this);
    }

    public static MessagePayload toMessagePayload(
            /* MessagePayload */ InvocationHandler invocationHandler) {
        if (invocationHandler == null) {
            return null;
        }
        WebMessagePayloadBoundaryInterface webMessagePayloadBoundaryInterface =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebMessagePayloadBoundaryInterface.class, invocationHandler);
        return new MessagePayload(webMessagePayloadBoundaryInterface.getAsString());
    }

    public static MessagePayload fromWebMessageBoundaryInterface(
            @NonNull WebMessageBoundaryInterface boundaryInterface) {
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                    boundaryInterface.getSupportedFeatures(),
                    Features.WEB_MESSAGE_GET_MESSAGE_PAYLOAD)) {
            // MessagePayload supported by supported lib.
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
}
