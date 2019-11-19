// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Used for Js Java interaction, to receive postMessage back to the injected JavaScript object.
 * When the native counterpart of this object is gone, we still don't know if this is ready for gc
 * since developer could hold a reference to it. So just cut the connection between native and Java.
 */
@JNINamespace("android_webview")
public class JsReplyProxy extends AwSupportLibIsomorphic {
    private long mNativeJsReplyProxy;

    private JsReplyProxy(long nativeJsReplyProxy) {
        mNativeJsReplyProxy = nativeJsReplyProxy;
    }

    /**
     * Post message to the injected JavaScript object. Note that it will drop message if the
     * injected object is gone.
     *
     * @param message a non-null String message post to the JavaScript object.
     */
    public void postMessage(@NonNull final String message) {
        if (mNativeJsReplyProxy == 0) return;
        PostTask.runOrPostTask(UiThreadTaskTraits.USER_VISIBLE,
                () -> JsReplyProxyJni.get().postMessage(mNativeJsReplyProxy, message));
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
        void postMessage(long nativeJsReplyProxy, String message);
    }
}
