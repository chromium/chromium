// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.JNINamespace;

/**
 * Stub implementation of the VR Shell.
 * Used when enable_gvr_services = false
 */
@JNINamespace("vr")
public class VrShellDelegate {
     /**
     * Called when the native library is first available.
     */
    public static void onNativeLibraryAvailable() {
    }
}
