// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Native bridge for credit card scanner. */
@JNINamespace("autofill")
@NullMarked
public class CreditCardScannerBridge implements CreditCardScanner.Delegate {
    @JniType("::autofill::CreditCardScannerViewAndroid")
    interface NativeCreditCardScannerViewAndroid extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeCreditCardScannerViewAndroid> mNativeScanner;
    private final CreditCardScanner mScanner;
    private final @Nullable IntentRequestTracker mIntentRequestTracker;
    private final WeakReference<Context> mContextRef;

    @CalledByNative
    private static CreditCardScannerBridge create(
            @Nullable JniRawPtr<NativeCreditCardScannerViewAndroid> nativeScanner,
            @Nullable WebContents webContents) {
        return new CreditCardScannerBridge(nativeScanner, webContents);
    }

    private CreditCardScannerBridge(
            @Nullable JniRawPtr<NativeCreditCardScannerViewAndroid> nativeScanner,
            @Nullable WebContents webContents) {
        mNativeScanner = nativeScanner;
        mScanner = CreditCardScanner.create(this);
        WindowAndroid window = webContents != null ? webContents.getTopLevelNativeWindow() : null;
        if (window != null) {
            mIntentRequestTracker = window.getIntentRequestTracker();
            mContextRef = window.getContext();
        } else {
            mIntentRequestTracker = null;
            mContextRef = new ImmutableWeakReference<>(null);
        }
    }

    @CalledByNative
    private boolean canScan() {
        return mScanner.canScan();
    }

    @CalledByNative
    private void scan() {
        mScanner.scan(mIntentRequestTracker);
    }

    @CalledByNative
    private void destroy() {
        if (mNativeScanner != null) {
            mNativeScanner.release();
            mNativeScanner = null;
        }
    }

    @Override
    public void onScanCancelled() {
        if (mNativeScanner != null) {
            CreditCardScannerBridgeJni.get().scanCancelled(mNativeScanner);
        }
    }

    @Override
    public void onScanCompleted(
            String cardHolderName, String cardNumber, int expirationMonth, int expirationYear) {
        if (mNativeScanner != null) {
            CreditCardScannerBridgeJni.get()
                    .scanCompleted(
                            mNativeScanner,
                            cardHolderName,
                            cardNumber,
                            expirationMonth,
                            expirationYear);
        }
    }

    @Override
    public @Nullable Context getWindowContext() {
        return mContextRef.get();
    }

    @NativeMethods
    interface Natives {
        void scanCancelled(JniPtr<NativeCreditCardScannerViewAndroid> self);

        void scanCompleted(
                JniPtr<NativeCreditCardScannerViewAndroid> self,
                @JniType("std::u16string") String cardHolderName,
                @JniType("std::u16string") String cardNumber,
                int expirationMonth,
                int expirationYear);
    }
}
