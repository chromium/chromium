// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import androidx.javascriptengine.common.Utils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxConsoleCallback;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateClient;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateSyncCallback;
import org.chromium.base.Log;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

import javax.annotation.concurrent.GuardedBy;

/** Service that provides methods for Javascript execution. */
@JNINamespace("android_webview")
public class JsSandboxIsolate extends IJsSandboxIsolate.Stub {
    private static final String TAG = "JsSandboxIsolate";
    // mLock must never be held whilst (synchronously) calling back into the client/embedding
    // application, otherwise it's entirely possible for the embedder to then call back into service
    // code (on another thread) and then try to take mLock again and therefore deadlock.
    private final Object mLock = new Object();
    private final JsSandboxService mService;
    private final AtomicReference<IJsSandboxConsoleCallback> mConsoleCallback =
            new AtomicReference<IJsSandboxConsoleCallback>();

    @GuardedBy("mLock")
    private long mJsSandboxIsolate;

    private final IJsSandboxIsolateClient mIsolateClient;

    JsSandboxIsolate(JsSandboxService service) {
        this(service, 0);
    }

    JsSandboxIsolate(JsSandboxService service, long maxHeapSizeBytes) {
        this(service, maxHeapSizeBytes, null);
    }

    JsSandboxIsolate(
            JsSandboxService service,
            long maxHeapSizeBytes,
            IJsSandboxIsolateClient isolateClient) {
        mService = service;
        mIsolateClient = isolateClient;
        mJsSandboxIsolate =
                JsSandboxIsolateJni.get()
                        .createNativeJsSandboxIsolateWrapper(this, maxHeapSizeBytes);
    }

    @Override
    public void evaluateJavascript(String code, IJsSandboxIsolateCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }
            JsSandboxIsolateJni.get()
                    .evaluateJavascript(
                            mJsSandboxIsolate, this, code, new JsSandboxIsolateCallback(callback));
        }
    }

    @Override
    public void evaluateJavascriptWithFd(
            AssetFileDescriptor afd, IJsSandboxIsolateSyncCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }

            Utils.checkAssetFileDescriptor(afd, /* allowUnknownLength= */ true);
            if (afd.getLength() > Integer.MAX_VALUE) {
                throw new IllegalArgumentException(
                        "Evaluation code larger than "
                                + Integer.MAX_VALUE
                                + " bytes not supported");
            }
            JsSandboxIsolateJni.get()
                    .evaluateJavascriptWithFd(
                            mJsSandboxIsolate,
                            this,
                            afd.getParcelFileDescriptor().getFd(),
                            afd.getLength(),
                            afd.getStartOffset(),
                            new JsSandboxIsolateFdCallback(callback),
                            afd.getParcelFileDescriptor());
        }
    }

    @Override
    public void close() {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                return;
            }
            JsSandboxIsolateJni.get().destroyNative(mJsSandboxIsolate, this);
            mJsSandboxIsolate = 0;
        }
    }

    @Override
    public boolean provideNamedData(String name, AssetFileDescriptor afd) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException(
                        "provideNamedData(String, AssetFileDescriptor) called after close()");
            }

            Utils.checkAssetFileDescriptor(afd, /* allowUnknownLength= */ false);
            if (afd.getLength() > Integer.MAX_VALUE) {
                throw new IllegalArgumentException(
                        "Named data larger than " + Integer.MAX_VALUE + " bytes not supported");
            }
            boolean nativeReturn =
                    JsSandboxIsolateJni.get()
                            .provideNamedData(
                                    mJsSandboxIsolate,
                                    this,
                                    name,
                                    afd.getParcelFileDescriptor().detachFd(),
                                    (int) afd.getLength());
            return nativeReturn;
        }
    }

    // Roughly truncate a (Unicode) Java string, avoiding truncation in the middle of a surrogate
    // pair. Note that this is fairly naive and doesn't deal with any additional complexities of
    // Unicode, such as characters composed of multiple code points, modifiers, ...
    //
    // maxCodePoints must be > 0.
    private static String truncateUnicodeString(String original, int maxLength) {
        if (original == null || original.length() <= maxLength) {
            return original;
        }
        if (Character.isHighSurrogate(original.charAt(maxLength - 1))) {
            maxLength--;
        }
        return original.substring(0, maxLength);
    }

    // Called by isolate thread
    @CalledByNative
    public void consoleMessage(
            int contextGroupId,
            int level,
            String message,
            String source,
            int line,
            int column,
            String trace) {
        final IJsSandboxConsoleCallback callback = mConsoleCallback.get();
        if (callback == null) {
            return;
        }
        // Note these are measured in chars (not bytes), so in the worst case the Binder parcel size
        // may be a little larger than 2 * (32768 + 4069 + 16348) = 106496.
        final int messageLimit = 32768;
        final int sourceLimit = 4096;
        final int traceLimit = 16384;
        message = truncateUnicodeString(message, messageLimit);
        source = truncateUnicodeString(source, sourceLimit);
        trace = truncateUnicodeString(trace, traceLimit);
        try {
            callback.consoleMessage(contextGroupId, level, message, source, line, column, trace);
        } catch (RemoteException e) {
            Log.e(TAG, "consoleMessage notification failed", e);
        }
    }

    // Called by isolate thread
    @CalledByNative
    public void consoleClear(int contextGroupId) {
        final IJsSandboxConsoleCallback callback = mConsoleCallback.get();
        if (callback == null) {
            return;
        }
        try {
            callback.consoleClear(contextGroupId);
        } catch (RemoteException e) {
            Log.e(TAG, "consoleClear notification failed", e);
        }
    }

    // Checks for errors thrown by client side while reading the stream and closes the Pfd.
    @CalledByNative
    private static String checkStreamingErrorAndClosePfd(ParcelFileDescriptor pfd) {
        try {
            if (pfd.canDetectErrors()) {
                try {
                    pfd.checkError();
                } catch (IOException e) {
                    // This streaming error would have already been thrown on the client side.
                    return e.toString();
                }
            }
        } finally {
            try {
                pfd.close();
            } catch (IOException e) {
                Log.e(TAG, "could not close Pfd", e);
            }
        }
        // Either Pfd is not associated with a reliablePipe or remote-side has no errors to report
        return null;
    }

    @Override
    public void setConsoleCallback(IJsSandboxConsoleCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("setConsoleCallback() called after close()");
            }
            mConsoleCallback.set(callback);
            JsSandboxIsolateJni.get().setConsoleEnabled(mJsSandboxIsolate, this, callback != null);
        }
    }

    // Notify the client side that the isolate should be terminated.
    //
    // Returns true if the client supports and received the onTerminated notification. (It is OK to
    // call this method regardless of whether the client supports the notification.)
    //
    // The service must ensure no other Binder calls (related to this isolate) are made back to the
    // client if this method returns true.
    @CalledByNative
    public boolean sendTermination(int status, String message) {
        if (mIsolateClient == null) {
            return false;
        }
        try {
            final String binderFriendlyMessage = truncateUnicodeString(message, 32768);
            mIsolateClient.onTerminated(status, binderFriendlyMessage);
            return true;
        } catch (RemoteException e) {
            // The client theoretically supports notifications, but probably didn't get it.
            // Ignoring this failure might cause the client to hang forever, so kill the whole
            // sandbox with an exception, which the client shouldn't ignore.
            throw new RuntimeException(e);
        }
    }

    public static void initializeEnvironment() {
        JsSandboxIsolateJni.get().initializeEnvironment();
    }

    @NativeMethods
    public interface Natives {
        long createNativeJsSandboxIsolateWrapper(
                JsSandboxIsolate jsSandboxIsolate, long maxHeapSizeBytes);

        void initializeEnvironment();

        // The calling code must not call any methods after it called destroyNative().
        void destroyNative(long nativeJsSandboxIsolate, JsSandboxIsolate caller);

        boolean evaluateJavascript(
                long nativeJsSandboxIsolate,
                JsSandboxIsolate caller,
                String script,
                JsSandboxIsolateCallback callback);

        boolean evaluateJavascriptWithFd(
                long nativeJsSandboxIsolate,
                JsSandboxIsolate caller,
                int fd,
                long length,
                long offset,
                JsSandboxIsolateFdCallback callback,
                ParcelFileDescriptor pfd);

        boolean provideNamedData(
                long nativeJsSandboxIsolate,
                JsSandboxIsolate caller,
                String name,
                int fd,
                int length);

        void setConsoleEnabled(
                long nativeJsSandboxIsolate, JsSandboxIsolate caller, boolean enable);
    }
}
