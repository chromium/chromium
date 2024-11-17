// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preloading;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

public class PreloadingDataBridge {
    public static void setIsNavigationInDomainCallbackForCct(WebContents webContents) {
        PreloadingDataBridgeJni.get().setIsNavigationInDomainCallbackForCct(webContents);
    }

    @NativeMethods
    interface Natives {
        void setIsNavigationInDomainCallbackForCct(
                @JniType("content::WebContents*") WebContents webContents);
    }
}
