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
        messagePortInternal.setClient(
                new MessagePortInternal.MessagePortClient() {
                    @Override
                    public void onString(String string) {
                        JsSandboxMessagePortJni.get()
                                .handleString(mNativeJsSandboxMessagePort, string);
                    }

                    @Override
                    public void onArrayBuffer(byte[] arrayBuffer) {
                        JsSandboxMessagePortJni.get()
                                .handleArrayBuffer(mNativeJsSandboxMessagePort, arrayBuffer);
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
        void handleString(long nativeJsSandboxMessagePort, @JniType("std::string") String string);

        void handleArrayBuffer(long nativeJsSandboxMessagePort, byte[] arrayBuffer);
    }
}
