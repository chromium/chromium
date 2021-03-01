// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previews;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Java bridge class to C++ Previews code.
 */
public final class PreviewsAndroidBridge {
    private static PreviewsAndroidBridge sBridge;

    public static PreviewsAndroidBridge getInstance() {
        if (sBridge == null) {
            sBridge = new PreviewsAndroidBridge();
        }
        return sBridge;
    }

    private final long mNativePreviewsAndroidBridge;

    private PreviewsAndroidBridge() {
        mNativePreviewsAndroidBridge =
                PreviewsAndroidBridgeJni.get().init(PreviewsAndroidBridge.this);
    }

    /**
     * Returns whether LiteMode https image compression is applied.
     */
    public boolean isHttpsImageCompressionApplied(WebContents webContents) {
        return PreviewsAndroidBridgeJni.get().isHttpsImageCompressionApplied(
                mNativePreviewsAndroidBridge, PreviewsAndroidBridge.this, webContents);
    }

    @CalledByNative
    private static boolean createHttpsImageCompressionInfoBar(final Tab tab) {
        return HttpsImageCompressionUtils.createInfoBar(tab);
    }

    @NativeMethods
    interface Natives {
        long init(PreviewsAndroidBridge caller);
        boolean isHttpsImageCompressionApplied(long nativePreviewsAndroidBridge,
                PreviewsAndroidBridge caller, WebContents webContents);
    }
}
