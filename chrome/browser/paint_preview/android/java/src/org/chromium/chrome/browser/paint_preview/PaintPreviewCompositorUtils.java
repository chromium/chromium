// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Utilities for warming up a compositor process for paint previews.
 */
@JNINamespace("paint_preview")
public class PaintPreviewCompositorUtils {
    /**
     * Warms up the compositor process.
     */
    static void warmupCompositor() {
        PaintPreviewCompositorUtilsJni.get().warmupCompositor();
    }

    /**
     * Stops the warm warm compositor process if one exists.
     */
    static boolean stopWarmCompositor() {
        return PaintPreviewCompositorUtilsJni.get().stopWarmCompositor();
    }

    @NativeMethods
    interface Natives {
        void warmupCompositor();
        boolean stopWarmCompositor();
    }

    private PaintPreviewCompositorUtils() {}
}


