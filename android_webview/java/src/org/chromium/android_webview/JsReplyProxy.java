// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePayloadType;
import org.chromium.js_injection.mojom.JavaScriptExecutionError;

/**
 * Used for Js Java interaction, to receive postMessage back to the injected JavaScript object. When
 * the native counterpart of this object is gone, we still don't know if this is ready for gc since
 * developer could hold a reference to it. So just cut the connection between native and Java.
 */
@JNINamespace("android_webview")
@NullMarked
public class JsReplyProxy extends AwSupportLibIsomorphic {
    @JniType("::android_webview::JsReplyProxy")
    interface NativeJsReplyProxy extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeJsReplyProxy> mNativeJsReplyProxy;

    private JsReplyProxy(JniRawPtr<NativeJsReplyProxy> nativeJsReplyProxy) {
        mNativeJsReplyProxy = nativeJsReplyProxy;
    }

    /**
     * Post message to the injected JavaScript object. Note that it will drop message if the
     * injected object is gone.
     *
     * @throws IllegalStateException if the payload is a {@link
     *     MessagePayloadType#SHARED_ARRAY_BUFFER} and the target frame, which is otherwise able to
     *     receive the message, is not cross-origin isolated.
     */
    public void postMessage(final MessagePayload payload) {
        if (mNativeJsReplyProxy == null) return;
        PostTask.runOrPostTask(
                TaskTraits.UI_USER_VISIBLE,
                () -> {
                    if (mNativeJsReplyProxy == null) return;
                    if (payload.getType() == MessagePayloadType.SHARED_ARRAY_BUFFER) {
                        Boolean isCrossOriginIsolated =
                                JsReplyProxyJni.get().isCrossOriginIsolated(mNativeJsReplyProxy);
                        if (isCrossOriginIsolated == null) {
                            // There is no frame to send the message to, so drop it. Reporting an
                            // isolation failure here would be wrong: the message would not have
                            // been delivered either way.
                            return;
                        }
                        if (!isCrossOriginIsolated) {
                            throw new IllegalStateException(
                                    "Cannot send SharedArrayBuffer to a frame that is not"
                                        + " cross-origin isolated. If this was intended, consider"
                                        + " allowing your origin with"
                                        + " `Profile#setCrossOriginIsolatedAllowlist()`, and add"
                                        + " the Document-Isolation-Policy header the page's"
                                        + " response.");
                        }
                    }
                    JsReplyProxyJni.get().postMessage(mNativeJsReplyProxy, payload);
                });
    }

    /**
     * Executes JavaScript in the same frame and world as the injected JavaScript object and returns
     * the result. The callback's onError will be called if the frame is gone or if the underlying
     * messagelistener that supported this proxy is removed.
     */
    public void executeJavaScript(
            final String script, @Nullable final JavaScriptExecutionCallback callback) {
        PostTask.runOrPostTask(
                TaskTraits.UI_USER_VISIBLE,
                () -> {
                    if (mNativeJsReplyProxy == null) {
                        if (callback != null) {
                            // If the proxy was destroyed, it's probably because the handler was
                            // removed.
                            callback.onError(JavaScriptExecutionError.FRAME_DESTROYED);
                        }
                        return;
                    }
                    JsReplyProxyJni.get().executeJavaScript(mNativeJsReplyProxy, script, callback);
                });
    }

    @CalledByNative
    private static JsReplyProxy create(JniRawPtr<NativeJsReplyProxy> nativeJsReplyProxy) {
        return new JsReplyProxy(nativeJsReplyProxy);
    }

    @CalledByNative
    private void onDestroy() {
        if (mNativeJsReplyProxy != null) {
            mNativeJsReplyProxy.release();
            mNativeJsReplyProxy = null;
        }
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(
            @JniType("std::u16string") String result,
            boolean success,
            @JniType("js_injection::mojom::JavaScriptExecutionError")
                    @JavaScriptExecutionError.EnumType
                    int errorCode,
            JavaScriptExecutionCallback callback) {
        if (success) {
            callback.onSuccess(result);
        } else {
            callback.onError(errorCode);
        }
    }

    @NativeMethods
    interface Natives {
        void postMessage(JniPtr<NativeJsReplyProxy> self, MessagePayload payload);

        void executeJavaScript(
                JniPtr<NativeJsReplyProxy> self,
                @JniType("std::u16string") String script,
                @Nullable JavaScriptExecutionCallback callback);

        // Null if there is no frame to post messages to.
        @JniType("std::optional<bool>")
        @Nullable Boolean isCrossOriginIsolated(JniPtr<NativeJsReplyProxy> self);
    }
}
