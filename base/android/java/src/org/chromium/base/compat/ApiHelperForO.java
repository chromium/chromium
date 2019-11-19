// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.content.ClipDescription;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.view.Display;
import android.view.View;

import org.chromium.base.annotations.VerifiesOnO;

/**
 * Utility class to use new APIs that were added in O (API level 26). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnO
@TargetApi(Build.VERSION_CODES.O)
public final class ApiHelperForO {
    private ApiHelperForO() {}

    /** See {@link Display#isWideColorGamut() }. */
    public static boolean isWideColorGamut(Display display) {
        return display.isWideColorGamut();
    }

    /** See {@link Configuration#isScreenWideColorGamut() }. */
    public static boolean isScreenWideColorGamut(Configuration configuration) {
        return configuration.isScreenWideColorGamut();
    }

    /** See {@link PackageManager#isInstantApp() }. */
    public static boolean isInstantApp(PackageManager packageManager) {
        return packageManager.isInstantApp();
    }

    /** See {@link View#setDefaultFocusHighlightEnabled(boolean) }. */
    public static void setDefaultFocusHighlightEnabled(View view, boolean enabled) {
        view.setDefaultFocusHighlightEnabled(enabled);
    }

    /** See {@link ClipDescription#getTimestamp()}. */
    public static long getTimestamp(ClipDescription clipDescription) {
        return clipDescription.getTimestamp();
    }
}
