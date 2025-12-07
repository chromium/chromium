// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;


import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultInfo;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A utility class for querying information about the default browser setting. */
@NullMarked
public final class DefaultBrowserInfoUmaRecorder {

    //  LINT.IfChange(MobileDefaultBrowserState)
    /**
     * A list of potential default browser states. To add a type to this list please update
     * MobileDefaultBrowserState in histograms.xml and make sure to keep this list in sync.
     * Additions should be treated as APPEND ONLY to keep the UMA metric semantics the same over
     * time.
     */
    @IntDef({
        MobileDefaultBrowserState.NO_DEFAULT,
        MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT,
        MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT,
        MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT,
        MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT,
        MobileDefaultBrowserState.OTHER_CHROME_DEFAULT,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface MobileDefaultBrowserState {
        int NO_DEFAULT = 0;
        int CHROME_SYSTEM_DEFAULT = 1;
        int CHROME_INSTALLED_DEFAULT = 2;
        int OTHER_SYSTEM_DEFAULT = 3;
        int OTHER_INSTALLED_DEFAULT = 4;
        int OTHER_CHROME_DEFAULT = 5;
        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/mobile/enums.xml:MobileDefaultBrowserState)

    /** Don't instantiate me. */
    private DefaultBrowserInfoUmaRecorder() {}

    /** Log statistics about the current default browser to UMA. */
    public static void logDefaultBrowserStats() {
        assert BrowserStartupController.getInstance().isFullBrowserStarted();

        DefaultBrowserInfo.resetDefaultInfoTask();
        DefaultBrowserInfo.getDefaultBrowserInfo(
                info -> {
                    if (info == null) return;

                    ChromeSharedPreferences.getInstance()
                            .writeBoolean(
                                    ChromePreferenceKeys.CHROME_DEFAULT_BROWSER,
                                    info.defaultBrowserState == DefaultBrowserState.CHROME_DEFAULT);
                    RecordHistogram.recordCount100Histogram(
                            getSystemBrowserCountUmaName(info), info.systemCount);
                    RecordHistogram.recordCount100Histogram(
                            getDefaultBrowserCountUmaName(info), info.browserCount);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Mobile.DefaultBrowser.State",
                            getDefaultBrowserUmaState(info),
                            MobileDefaultBrowserState.NUM_ENTRIES);
                });
    }

    private static String getSystemBrowserCountUmaName(DefaultInfo info) {
        if (info.isChromeSystem) return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeSystem";
        return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeNotSystem";
    }

    private static String getDefaultBrowserCountUmaName(DefaultInfo info) {
        if (info.defaultBrowserState == DefaultBrowserState.NO_DEFAULT) {
            return "Mobile.DefaultBrowser.BrowserCount.NoDefault";
        }
        if (info.defaultBrowserState == DefaultBrowserState.CHROME_DEFAULT) {
            return "Mobile.DefaultBrowser.BrowserCount.ChromeDefault";
        }
        return "Mobile.DefaultBrowser.BrowserCount.OtherDefault";
    }

    private static @MobileDefaultBrowserState int getDefaultBrowserUmaState(DefaultInfo info) {
        switch (info.defaultBrowserState) {
            case DefaultBrowserState.NO_DEFAULT:
                return MobileDefaultBrowserState.NO_DEFAULT;
            case DefaultBrowserState.CHROME_DEFAULT:
                if (info.isDefaultSystem) return MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT;
                return MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT;
            case DefaultBrowserState.OTHER_CHROME_DEFAULT:
                return MobileDefaultBrowserState.OTHER_CHROME_DEFAULT;
            case DefaultBrowserState.OTHER_DEFAULT:
                if (info.isDefaultSystem) {
                    return MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT;
                }
                return MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT;
        }

        assert false;
        return MobileDefaultBrowserState.NO_DEFAULT;
    }
}
