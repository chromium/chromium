// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.os.RemoteException;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Java endpoint representing an individual message port connected to an external native Android
 * application. TODO(crbug.com/515159909): Hook this up as the Java counterpart of
 * native_message_android_port.h/.cc and to the rest of the extensions system. This class will be
 * owned by C++ NativeMessageAndroidPort though it is also referenced by other Java classes.
 */
@JNINamespace("extensions")
@NullMarked
public class NativeMessageAndroidPort {
    // An observer interface for classes which keep a reference to this class
    // but do not own it, like ExtensionSession.
    public interface Observer {
        // Called when the `port` is being destroyed by its owning class.
        // Observers should release their references.
        void onPortDestroying(NativeMessageAndroidPort port);
    }

    // A test interface to observe events in unit tests, such as when the port
    // receives a message from an external app or when it's closed.
    public interface TestObserver {
        void onMessageFromApp(String message);

        void onChannelClosed(String errorMessage);
    }

    private static final String TAG = "NMAndroidPort";
    private long mNativePtr;
    private @Nullable Observer mObserver;
    private @Nullable TestObserver mTestObserver;

    // Listens for incoming calls from the external app and forwards them back
    // to the browser. Populated while a connection attempt is in flight or
    // actively connected.
    private @Nullable Callback mCallback;

    // Forwards outgoing messages from the browser to the external app.
    // Populated only when this port is actively connected to the app.
    private @Nullable IExtensionNativeMessagePort mRemotePort;

    private final List<String> mPendingMessages = new ArrayList<>();

    @CalledByNative
    private static NativeMessageAndroidPort create(long nativePtr) {
        return new NativeMessageAndroidPort(nativePtr);
    }

    @VisibleForTesting
    public NativeMessageAndroidPort() {
        this(0);
    }

    private NativeMessageAndroidPort(long nativePtr) {
        mNativePtr = nativePtr;
    }

    public void setObserver(Observer observer) {
        mObserver = observer;
    }

    @VisibleForTesting
    public void setTestObserver(@Nullable TestObserver observer) {
        mTestObserver = observer;
    }

    // Initiates connecting this port (which is owned by the extension with the given `extensionId`)
    // to the external app. Returns an error message if the connection immediately fails, or null on
    // success.
    @CalledByNative
    public @Nullable String connectToApp(
            Profile profile, String packageName, String extensionId, boolean isVerifiedExtension) {
        NativeMessagingManager manager = NativeMessagingManager.getForProfile(profile);
        return manager.addPort(packageName, extensionId, isVerifiedExtension, this);
    }

    // Sets the active callback for this port.
    public void setCallback(@Nullable Callback callback) {
        mCallback = callback;
    }

    // Called when the external app accepts the browser's connection request for
    // a remote port initiated from `connectToApp`.
    public void onConnected(IExtensionNativeMessagePort remotePort) {
        mRemotePort = remotePort;

        for (String message : new ArrayList<>(mPendingMessages)) {
            send(message);
            if (mRemotePort == null) {
                // `send()` can fail, causing the channel to close and `mRemotePort`
                // to be nulled out. Abort if that's the case.
                break;
            }
        }
        mPendingMessages.clear();
    }

    // Called to send a message to the external app. If this port is not yet connected to the app's
    // `mRemotePort` receiver then the message is put in a pending queue.
    @CalledByNative
    public void forwardMessageToApp(String message) {
        if (mRemotePort != null) {
            send(message);
        } else {
            mPendingMessages.add(message);
        }
    }

    // Called when the port is being destroyed.
    @CalledByNative
    public void destroy() {
        mNativePtr = 0;
        if (mObserver != null) {
            mObserver.onPortDestroying(this);
            mObserver = null;
        }

        if (mCallback != null) {
            mCallback.mPort = null;
            mCallback = null;
        }

        if (mRemotePort != null) {
            try {
                mRemotePort.disconnect();
            } catch (RemoteException e) {
                Log.w(TAG, "Failed to disconnect from the app", e);
            } finally {
                mRemotePort = null;
            }
        }
    }

    // Called by Callback when the external app posts a message back to the
    // extension.
    public void postMessageFromApp(String message) {
        if (mTestObserver != null) {
            mTestObserver.onMessageFromApp(message);
        }

        if (mNativePtr != 0) {
            NativeMessageAndroidPortJni.get().postMessageFromApp(mNativePtr, message);
        }
    }

    // Called when the port connection is closed by the external app, on
    // connection failure, or during session teardown / browser shutdown.
    public void closeChannel(String errorMessage) {
        if (mTestObserver != null) {
            mTestObserver.onChannelClosed(errorMessage);
        }

        mPendingMessages.clear();
        if (mNativePtr != 0) {
            // This will destroy the C++ NativeMessageAndroidPort which will
            // call destroy().
            NativeMessageAndroidPortJni.get().closeChannel(mNativePtr, errorMessage);
        }
    }

    private void send(String message) {
        assert mRemotePort != null;
        try {
            mRemotePort.postMessage(message);
        } catch (RemoteException e) {
            Log.w(TAG, "Failed to post message to external app", e);
            closeChannel("Error when communicating with the native messaging host.");
        }
    }

    // Native methods implemented in C++ (NativeMessageAndroidPort.cc) called from Java to C++.
    @NativeMethods
    interface Natives {
        // Forwards a message received from the external Android app to the C++
        // NativeMessageAndroidPort, which delivers it to the extension.
        void postMessageFromApp(long nativeNativeMessageAndroidPort, String message);

        // Notifies the C++ NativeMessageAndroidPort that the channel has been closed
        // (e.g. by the app, due to an error, or during teardown), closing the port
        // and dispatching any error message to the extension.
        void closeChannel(long nativeNativeMessageAndroidPort, String errorMessage);
    }

    // A helper that receives calls from the external app back to the browser
    // and forwards these calls back to the NativeMessageAndroidPort.
    public static class Callback extends IExtensionNativeMessageCallback.Stub {
        private static final String TAG = "NMCallback";

        private @Nullable NativeMessageAndroidPort mPort;

        public Callback(NativeMessageAndroidPort port) {
            mPort = port;
            port.setCallback(this);
        }

        // IExtensionNativeMessageCallback.onMessage implementation. Called on a
        // binder thread so post a task to the UI thread where `mPort` lives.
        @Override
        public void onMessage(String message) {
            if (message != null) {
                ThreadUtils.postOnUiThread(
                        () -> {
                            if (mPort != null) {
                                mPort.postMessageFromApp(message);
                            }
                        });
            }
        }

        // IExtensionNativeMessageCallback.onDisconnect implementation. Called
        // on a binder thread so post a task to the UI thread where `mPort`
        // lives.
        @Override
        public void onDisconnect() {
            // Close the channel with an empty error message since the external
            // app likely deliberately closed the connection.
            ThreadUtils.postOnUiThread(
                    () -> {
                        NativeMessageAndroidPort port = mPort;
                        mPort = null;
                        if (port != null) {
                            // Null out the remote port to prevent any further
                            // communications with the app through this port.
                            port.mRemotePort = null;
                            port.closeChannel("");
                        }
                    });
        }
    }
}
