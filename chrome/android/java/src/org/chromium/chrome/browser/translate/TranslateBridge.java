// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Bridge class that lets Android code access native code to execute translate on a tab.
 */
public class TranslateBridge {
    /**
     * Translates the given tab.
     */
    public static void translateTab(Tab tab) {
        nativeTranslate(tab.getWebContents());
    }

    /**
     * Returns true iff the current tab can be manually translated.
     */
    public static boolean canManuallyTranslate(Tab tab) {
        return nativeCanManuallyTranslate(tab.getWebContents());
    }

    private static native void nativeTranslate(WebContents webContents);
    private static native boolean nativeCanManuallyTranslate(WebContents webConcents);
}
