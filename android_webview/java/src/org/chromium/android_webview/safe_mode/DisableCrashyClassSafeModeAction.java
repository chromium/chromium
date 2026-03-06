// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_mode;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.build.annotations.NullMarked;

/**
 * This safe mode is for testing purposes only. It should never be deployed in production.
 * DisableCrashyClassSafeModeAction should be used in conjunction with the following flags/switches:
 *
 * <ul>
 *   <li>AwFeatures.WEBVIEW_ENABLE_CRASH & AwSwitches.WEBVIEW_FORCE_CRASH_JAVA or
 *   <li>AwFeatures.WEBVIEW_ENABLE_CRASH & AwSwitches.WEBVIEW_FORCE_CRASH_NATIVE
 * </ul>
 *
 * When one or both of those feature/switch combinations are enabled, WebView will crash
 * immediately. This safe mode action prevents that crash and should be used to test the efficacy of
 * safe mode deployments.
 */
@Lifetime.Singleton
@NullMarked
public class DisableCrashyClassSafeModeAction implements SafeModeAction {
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_CRASHY_CLASS;

    private static boolean sShouldDisableCrashyClass;

    @Override
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        sShouldDisableCrashyClass = true;
        return true;
    }

    public static boolean shouldDisableCrashyClass() {
        return sShouldDisableCrashyClass;
    }
}
