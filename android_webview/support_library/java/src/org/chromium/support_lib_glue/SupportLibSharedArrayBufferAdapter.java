// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.system.ErrnoException;

import org.jspecify.annotations.NullMarked;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.content_public.browser.SharedArrayBuffer;
import org.chromium.support_lib_boundary.SharedArrayBufferBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;
import java.nio.ByteBuffer;

/** Adapter between SharedArrayBufferBoundaryInterface and SharedArrayBuffer in content/. */
@Lifetime.Temporary
@NullMarked
class SupportLibSharedArrayBufferAdapter implements SharedArrayBufferBoundaryInterface {
    private final SharedArrayBuffer mSharedArrayBuffer;

    public SupportLibSharedArrayBufferAdapter(SharedArrayBuffer sharedArrayBuffer) {
        mSharedArrayBuffer = sharedArrayBuffer;
    }

    private SharedArrayBuffer getSharedArrayBuffer() {
        return mSharedArrayBuffer;
    }

    @Override
    public ByteBuffer mapAndCreateByteBuffer() throws ErrnoException {
        return mSharedArrayBuffer.mapAndCreateByteBuffer();
    }

    @Override
    public long getSize() {
        return mSharedArrayBuffer.getSize();
    }

    public static /* SharedArrayBuffer */ InvocationHandler getInvocationHandler(
            SharedArrayBuffer sharedArrayBuffer) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibSharedArrayBufferAdapter(sharedArrayBuffer));
    }

    public static SharedArrayBuffer toSharedArrayBuffer(
            /* SharedArrayBuffer */ InvocationHandler invocationHandler) {
        SupportLibSharedArrayBufferAdapter adapter =
                (SupportLibSharedArrayBufferAdapter)
                        BoundaryInterfaceReflectionUtil.getDelegateFromInvocationHandler(
                                invocationHandler);
        return adapter.getSharedArrayBuffer();
    }
}
