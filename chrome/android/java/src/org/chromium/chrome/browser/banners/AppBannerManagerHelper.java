// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * This class is a helper class for the C++ layer. It responds whether the
 * feature is supported (and facilitates testing). It was split out of
 * AppBannerManager during a modularization effort because ShortcutHelper hadn't
 * been modularized. Once that is done, this functionality can move back into
 * AppBannerManager.
 */
@JNINamespace("banners")
public class AppBannerManagerHelper {
    /** Whether add to home screen is permitted by the system. */
    private static Boolean sIsSupported;

    /**
     * Checks if the add to home screen intent is supported.
     * @return true if add to home screen is supported, false otherwise.
     */
    public static boolean isSupported() {
        // TODO(mthiesse, https://crbug.com/840811): Support the app banner dialog in VR.
        if (VrModuleProvider.getDelegate().isInVr()) return false;
        if (sIsSupported == null) {
            sIsSupported = ShortcutHelper.isAddToHomeIntentSupported();
        }
        return sIsSupported;
    }

    /**
     * Checks if app banners are enabled for the tab which this manager is attached to.
     * @return true if app banners can be shown for this tab, false otherwise.
     */
    @CalledByNative
    private static boolean isEnabledForTab() {
        return isSupported();
    }

    /** Overrides whether the system supports add to home screen. Used in testing. */
    @VisibleForTesting
    public static void setIsSupported(boolean state) {
        sIsSupported = state;
    }
}
