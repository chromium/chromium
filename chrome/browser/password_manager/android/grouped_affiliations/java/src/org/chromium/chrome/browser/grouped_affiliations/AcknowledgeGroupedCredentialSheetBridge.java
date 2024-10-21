// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;

import org.chromium.ui.base.WindowAndroid;

// JNI bridge to display the acknowledgement sheet when filling grouped
// credentials on Android.
public class AcknowledgeGroupedCredentialSheetBridge {
    @CalledByNative
    public AcknowledgeGroupedCredentialSheetBridge(
            long nativeAddUsernameDialogBridge, @NonNull WindowAndroid windowAndroid) {}

    @CalledByNative
    public void show() {
        // TODO(crbug.com/372635361): Implement.
    }

    @CalledByNative
    public void dismiss() {
        // TODO(crbug.com/372635361): Implement.
    }
}
