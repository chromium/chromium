// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for History-related feature flags and conditions on Android. */
@NullMarked
public final class HistoryFeatures {
    private HistoryFeatures() {}

    /** Returns whether app-specific history is enabled. */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public static boolean isAppSpecificHistoryEnabled() {
        return ChromeFeatureList.sAppSpecificHistory.isEnabled()
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
    }

    // LINT.IfChange(AndroidDesktopWebUiHistory)
    /** Returns whether the WebUI history surface is enabled on Android Desktop. */
    public static boolean isAndroidDesktopWebUiHistoryEnabled() {
        return DeviceInfo.isDesktop() && ChromeFeatureList.sAndroidDesktopWebUiHistory.isEnabled();
    }
    // LINT.ThenChange(//chrome/browser/android/ntp/new_tab_page_url_handler.cc:AndroidDesktopWebUiHistory, //chrome/browser/ui/webui/history/history_ui.cc:AndroidDesktopWebUiHistory)
}
