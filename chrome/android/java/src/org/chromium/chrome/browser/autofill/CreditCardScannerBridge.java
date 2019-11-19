// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/** Native bridge for credit card scanner. */
@JNINamespace("autofill")
public class CreditCardScannerBridge implements CreditCardScanner.Delegate {
    private final long mNativeScanner;
    private final CreditCardScanner mScanner;

    @CalledByNative
    private static CreditCardScannerBridge create(long nativeScanner, WebContents webContents) {
        return new CreditCardScannerBridge(nativeScanner, webContents);
    }

    private CreditCardScannerBridge(long nativeScanner, WebContents webContents) {
        mNativeScanner = nativeScanner;
        mScanner = CreditCardScanner.create(webContents, this);
    }

    @CalledByNative
    private boolean canScan() {
        return mScanner.canScan();
    }

    @CalledByNative
    private void scan() {
        mScanner.scan();
    }

    @Override
    public void onScanCancelled() {
        CreditCardScannerBridgeJni.get().scanCancelled(
                mNativeScanner, CreditCardScannerBridge.this);
    }

    @Override
    public void onScanCompleted(
            String cardHolderName, String cardNumber, int expirationMonth, int expirationYear) {
        CreditCardScannerBridgeJni.get().scanCompleted(mNativeScanner, CreditCardScannerBridge.this,
                cardHolderName, cardNumber, expirationMonth, expirationYear);
    }

    @NativeMethods
    interface Natives {
        void scanCancelled(long nativeCreditCardScannerViewAndroid, CreditCardScannerBridge caller);
        void scanCompleted(long nativeCreditCardScannerViewAndroid, CreditCardScannerBridge caller,
                String cardHolderName, String cardNumber, int expirationMonth, int expirationYear);
    }
}
