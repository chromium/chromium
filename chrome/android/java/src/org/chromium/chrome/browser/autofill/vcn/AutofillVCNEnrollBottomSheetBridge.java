// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/** Bridge for the virtual card enrollment bottom sheet. */
@JNINamespace("autofill")
/*package*/ class AutofillVCNEnrollBottomSheetBridge {
    @CalledByNative
    private AutofillVCNEnrollBottomSheetBridge() {}

    @CalledByNative
    private boolean requestShowContent(
            long nativeAutofillVCNEnrollBottomSheetBridge, WebContents webContents) {
        return false;
    }
}
