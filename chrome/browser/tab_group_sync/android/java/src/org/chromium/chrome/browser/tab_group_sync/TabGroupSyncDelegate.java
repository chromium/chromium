// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Android implementation for TabGroupSyncDelegate. Owned by native. Internal to {@link
 * TabGroupSyncService} and responsible for being the glue layer between the service and tab model
 * in all windows. TODO(crbug.com/379699409): Finish implementation.
 */
@JNINamespace("tab_groups")
public class TabGroupSyncDelegate {
    @CalledByNative
    static TabGroupSyncDelegate create(long nativePtr) {
        return new TabGroupSyncDelegate(nativePtr);
    }

    private TabGroupSyncDelegate(long nativePtr) {
        assert nativePtr != 0;
        // TODO(crbug.com/379699409): Finish implementation.
    }

    @CalledByNative
    void destroy() {
        // TODO(crbug.com/379699409): Finish implementation.
    }
}
