// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;

/**
 * A browser-process class for querying SafeMode state and executing SafeModeActions.
 */
public class SafeModeController {
    public static final String SAFE_MODE_STATE_COMPONENT =
            "org.chromium.android_webview.SafeModeState";

    private SafeModeController() {}

    private static class LazyHolder {
        static final SafeModeController INSTANCE = new SafeModeController();
    }

    public static SafeModeController getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Quickly determine whether SafeMode is enabled. SafeMode is off-by-default.
     *
     * @param webViewPackageName the package name of the WebView implementation to query about
     *     SafeMode (generally this is the current WebView provider).
     */
    public boolean isSafeModeEnabled(String webViewPackageName) {
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent =
                new ComponentName(webViewPackageName, SAFE_MODE_STATE_COMPONENT);
        int enabledState =
                context.getPackageManager().getComponentEnabledSetting(safeModeComponent);
        return enabledState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
    }
}
