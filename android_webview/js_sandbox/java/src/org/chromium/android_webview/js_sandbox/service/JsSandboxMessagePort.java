// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import androidx.javascriptengine.common.MessagePortInternal;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("android_webview")
@NullMarked
public class JsSandboxMessagePort {
    private final long mNativeJsSandboxMessagePort;
    private final MessagePortInternal mMessagePortInternal;

    @CalledByNative
    public static JsSandboxMessagePort create(
            MessagePortInternal messagePortInternal, long nativeSandboxMessagePort) {
        JsSandboxMessagePort jsSandboxMessagePort =
                new JsSandboxMessagePort(messagePortInternal, nativeSandboxMessagePort);
        return jsSandboxMessagePort;
    }

    private JsSandboxMessagePort(
            MessagePortInternal messagePortInternal, long nativeJsSandboxMessagePort) {
        mNativeJsSandboxMessagePort = nativeJsSandboxMessagePort;
        // TODO(b/435619571):
        //  Memory allocation checks should ideally happen before arbitrary size data is received
        //  (via FDs), as this could easily exhaust memory before the MessagePortClient is invoked.
        messagePortInternal.setClient(
                new MessagePortInternal.MessagePortClient() {
                    @Override
                    public void onString(String string) {
                        // Note that the message may not always be stored as UTF-16. When stored in
                        // UTF-8, it may end up being more or less. As such, this is somewhat more
                        // of a best-effort estimate than a concrete value.
                        long size = (long) string.length() * Character.BYTES;
                        if (!JsSandboxMessagePortJni.get()
                                .tryAllocateMemoryBudget(mNativeJsSandboxMessagePort, size)) {
                            return;
                        }

                        JsSandboxMessagePortJni.get()
                                .handleString(mNativeJsSandboxMessagePort, string, size);
                    }

                    @Override
                    public void onArrayBuffer(byte[] arrayBuffer) {
                        long size = arrayBuffer.length;
                        if (!JsSandboxMessagePortJni.get()
                                .tryAllocateMemoryBudget(mNativeJsSandboxMessagePort, size)) {
                            return;
                        }

                        JsSandboxMessagePortJni.get()
                                .handleArrayBuffer(mNativeJsSandboxMessagePort, arrayBuffer, size);
                    }
                });
        mMessagePortInternal = messagePortInternal;
    }

    // Called by isolate thread
    @CalledByNative
    void postString(@JniType("std::string") String string) {
        mMessagePortInternal.postString(string);
    }

    // Called by isolate thread
    @CalledByNative
    void postArrayBuffer(byte[] arrayBuffer) {
        mMessagePortInternal.postArrayBuffer(arrayBuffer);
    }

    // Called by isolate thread
    @CalledByNative
    void close() {
        mMessagePortInternal.close();
    }

    @NativeMethods
    public interface Natives {
        void handleString(
                long nativeJsSandboxMessagePort, @JniType("std::string") String string, long size);

        void handleArrayBuffer(long nativeJsSandboxMessagePort, byte[] arrayBuffer, long size);

        boolean tryAllocateMemoryBudget(long nativeJsSandboxMessagePort, long size);
    }
}
