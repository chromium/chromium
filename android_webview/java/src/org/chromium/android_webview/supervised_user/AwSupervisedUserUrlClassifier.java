// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.supervised_user;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.AwSupervisedUserUrlClassifierDelegate;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/**
 * This class is used for determining if the current android user can access a given url. It
 * provides the link between native code and GMS Core where the actual url check takes place. Note
 * that the the user may change whether they are supervised or not between calls, but this is
 * handled on the GMS side.
 *
 * <p>Additionally supervised status is per Android Profile, so will be shared by all WebView
 * Profiles. There is currently no WebView/WebView Profile specific customisation allowed.
 *
 * <p>All of these methods can be called on any thread.
 *
 * <p>Lifetime: Singleton
 */
@JNINamespace("android_webview")
@NullMarked
public class AwSupervisedUserUrlClassifier {
    private static @Nullable AwSupervisedUserUrlClassifier sInstance;
    private static final Object sInstanceLock = new Object();
    private static boolean sInitialized;

    private final AwSupervisedUserUrlClassifierDelegate mDelegate;

    private AwSupervisedUserUrlClassifier(AwSupervisedUserUrlClassifierDelegate delegate) {
        mDelegate = delegate;
    }

    public static @Nullable AwSupervisedUserUrlClassifier getInstance() {
        // Supervised user filters currently do not function in the SDK sandbox.
        // See https://crbug.com/1523530.
        if (ContextUtils.isSdkSandboxProcess()) return null;

        synchronized (sInstanceLock) {
            if (!sInitialized) {
                AwSupervisedUserUrlClassifierDelegate delegate =
                        PlatformServiceBridge.getInstance().getUrlClassifierDelegate();
                if (delegate != null) {
                    sInstance = new AwSupervisedUserUrlClassifier(delegate);
                }
                sInitialized = true;
            }

            return sInstance;
        }
    }

    public static void resetInstanceForTesting() {
        synchronized (sInstanceLock) {
            sInstance = null;
            sInitialized = false;
        }
    }

    public void checkIfNeedRestrictedContentBlocking() {
        mDelegate.needsRestrictedContentBlocking(
                result -> {
                    ThreadUtils.postOnUiThread(
                            () -> {
                                AwSupervisedUserUrlClassifierJni.get()
                                        .setUserRequiresUrlChecks(result);
                            });
                });
    }

    @CalledByNative
    public static boolean shouldCreateThrottle() {
        return (getInstance() != null);
    }

    @CalledByNative
    public static void shouldBlockUrl(GURL requestUrl, JniOnceCallback<Boolean> callback) {
        // This should only be called if shouldCreateThrottle returns true.
        assumeNonNull(getInstance())
                .mDelegate
                .shouldBlockUrl(
                        requestUrl,
                        shouldBlockUrl -> {
                            ThreadUtils.postOnUiThread(() -> callback.onResult(shouldBlockUrl));
                        });
    }

    @NativeMethods
    interface Natives {
        void setUserRequiresUrlChecks(boolean userRequiresUrlChecks);
    }
}
