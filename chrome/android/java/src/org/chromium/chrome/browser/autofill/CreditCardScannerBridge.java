// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;

/** Native bridge for credit card scanner. */
@JNINamespace("autofill")
@NullMarked
public class CreditCardScannerBridge implements CreditCardScanner.Delegate {
    private final long mNativeScanner;
    private final CreditCardScanner mScanner;
    private final @Nullable IntentRequestTracker mIntentRequestTracker;

    @CalledByNative
    private static CreditCardScannerBridge create(long nativeScanner, WebContents webContents) {
        return new CreditCardScannerBridge(nativeScanner, webContents);
    }

    private CreditCardScannerBridge(long nativeScanner, WebContents webContents) {
        mNativeScanner = nativeScanner;
        mScanner = CreditCardScanner.create(this);
        if (webContents != null && webContents.getTopLevelNativeWindow() != null) {
            mIntentRequestTracker = webContents.getTopLevelNativeWindow().getIntentRequestTracker();
        } else {
            mIntentRequestTracker = null;
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

    @Override
    public void onScanCancelled() {
        CreditCardScannerBridgeJni.get().scanCancelled(mNativeScanner);
    }

    @Override
    public void onScanCompleted(
            String cardHolderName, String cardNumber, int expirationMonth, int expirationYear) {
        CreditCardScannerBridgeJni.get()
                .scanCompleted(
                        mNativeScanner,
                        cardHolderName,
                        cardNumber,
                        expirationMonth,
                        expirationYear);
    }

    @NativeMethods
    interface Natives {
        void scanCancelled(long nativeCreditCardScannerViewAndroid);

        void scanCompleted(
                long nativeCreditCardScannerViewAndroid,
                @JniType("std::u16string") String cardHolderName,
                @JniType("std::u16string") String cardNumber,
                int expirationMonth,
                int expirationYear);
    }
}
