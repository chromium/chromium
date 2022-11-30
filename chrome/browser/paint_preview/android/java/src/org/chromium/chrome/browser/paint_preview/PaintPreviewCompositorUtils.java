// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import androidx.annotation.VisibleForTesting;

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
    public static void warmupCompositor() {
        PaintPreviewCompositorUtilsJni.get().warmupCompositor();
    }

    /**
     * Stops the warm warm compositor process if one exists.
     * @return true if a warm compositor was present and stopped.
     */
    public static boolean stopWarmCompositor() {
        return PaintPreviewCompositorUtilsJni.get().stopWarmCompositor();
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void warmupCompositor();
        boolean stopWarmCompositor();
    }

    private PaintPreviewCompositorUtils() {}
}


