// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java bridge to delegate calls from native MandatoryReauthOptInViewAndroid. Facilitates creating
 * the Mandatory Reauth opt-in bottom sheet.
 */
@JNINamespace("autofill")
public class MandatoryReauthOptInBottomSheetViewBridge {
    private MandatoryReauthOptInBottomSheetViewBridge() {}

    /**
     * Creates an instance of a {@link MandatoryReauthOptInBottomSheetViewBridge}.
     */
    @CalledByNative
    private static MandatoryReauthOptInBottomSheetViewBridge create() {
        return new MandatoryReauthOptInBottomSheetViewBridge();
    }

    /**
     * Create and show the view.
     */
    @CalledByNative
    private void show() {}

    /**
     * Lets the native controller dismiss the view.
     */
    @CalledByNative
    private void close() {}
}
