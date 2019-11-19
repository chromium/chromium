// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.resources;

import android.graphics.Rect;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Utility class for creating native resources.
 */
@JNINamespace("android")
public class ResourceFactory {
    public static long createToolbarContainerResource(
            Rect toolbarPosition, Rect locationBarPosition, int shadowHeight) {
        return ResourceFactoryJni.get().createToolbarContainerResource(toolbarPosition.left,
                toolbarPosition.top, toolbarPosition.right, toolbarPosition.bottom,
                locationBarPosition.left, locationBarPosition.top, locationBarPosition.right,
                locationBarPosition.bottom, shadowHeight);
    }

    @NativeMethods
    interface Natives {
        long createToolbarContainerResource(int toolbarLeft, int toolbarTop, int toolbarRight,
                int toolbarBottom, int locationBarLeft, int locationBarTop, int locationBarRight,
                int locationBarBottom, int shadowHeight);
    }
}
