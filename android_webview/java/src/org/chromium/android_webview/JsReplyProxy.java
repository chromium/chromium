// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.MessagePayload;

/**
 * Used for Js Java interaction, to receive postMessage back to the injected JavaScript object. When
 * the native counterpart of this object is gone, we still don't know if this is ready for gc since
 * developer could hold a reference to it. So just cut the connection between native and Java.
 */
@JNINamespace("android_webview")
@NullMarked
public class JsReplyProxy extends AwSupportLibIsomorphic {
    private long mNativeJsReplyProxy;

    private JsReplyProxy(long nativeJsReplyProxy) {
        mNativeJsReplyProxy = nativeJsReplyProxy;
    }

    /**
     * Post message to the injected JavaScript object. Note that it will drop message if the
     * injected object is gone.
     */
    public void postMessage(final MessagePayload payload) {
        if (mNativeJsReplyProxy == 0) return;
        PostTask.runOrPostTask(
                TaskTraits.UI_USER_VISIBLE,
                () -> {
                    if (mNativeJsReplyProxy == 0) return;
                    JsReplyProxyJni.get().postMessage(mNativeJsReplyProxy, payload);
                });
    }

    @CalledByNative
    private static JsReplyProxy create(long nativeJsReplyProxy) {
        return new JsReplyProxy(nativeJsReplyProxy);
    }

    @CalledByNative
    private void onDestroy() {
        mNativeJsReplyProxy = 0;
    }

    @NativeMethods
    interface Natives {
        void postMessage(long nativeJsReplyProxy, MessagePayload payload);
    }
}
