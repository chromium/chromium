// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** JNI wrapper to trigger Android bottom sheet prompting the user to save their IBAN locally. */
@JNINamespace("autofill")
class AutofillSaveIbanBottomSheetBridge {
    private long mNativeAutofillSaveIbanBottomSheetBridge;

    @CalledByNative
    @VisibleForTesting
    AutofillSaveIbanBottomSheetBridge(long nativeAutofillSaveIbanBottomSheetBridge) {
        mNativeAutofillSaveIbanBottomSheetBridge = nativeAutofillSaveIbanBottomSheetBridge;
    }

    @CalledByNative
    public void requestShowContent(String ibanLabel) {}
}
