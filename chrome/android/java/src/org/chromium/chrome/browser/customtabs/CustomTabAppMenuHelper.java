// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIPHController;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.history.HistoryManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;

/** Helper class for custom tab app menu. */
public class CustomTabAppMenuHelper {
    private static boolean sAppHistoryEnabled = HistoryManager.isAppSpecificHistoryEnabled();

    /** Returns {@link CustomTabHistoryIPHController} if history menu is enabled on CCT. */
    public static CustomTabHistoryIPHController maybeCreateHistoryIPHController(
            AppMenuCoordinator appMenuCoordinator,
            Activity activity,
            ActivityTabProvider activityTabProvider,
            Supplier<Profile> profileSupplier,
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (appMenuCoordinator == null) return null;

        boolean hasPackageName = intentDataProvider.getClientPackageNameIdentitySharing() != null;
        return showHistoryItem(hasPackageName, intentDataProvider.getUiType())
                ? new CustomTabHistoryIPHController(
                        activity,
                        activityTabProvider,
                        profileSupplier,
                        appMenuCoordinator.getAppMenuHandler())
                : null;
    }

    /** Whether CCT will show the history item on the 3-dot menu. */
    public static boolean showHistoryItem(boolean hasClientPackage, @CustomTabsUiType int uiType) {
        if (!sAppHistoryEnabled || !hasClientPackage || !FirstRunStatus.getFirstRunFlowComplete()) {
            return false;
        }
        return switch (uiType) {
            case CustomTabsUiType.MEDIA_VIEWER,
                    CustomTabsUiType.READER_MODE,
                    CustomTabsUiType.MINIMAL_UI_WEBAPP,
                    CustomTabsUiType.OFFLINE_PAGE,
                    CustomTabsUiType.AUTH_TAB -> false;
            default -> true;
        };
    }

    static void setAppHistoryEnabledForTesting(boolean enabled) {
        sAppHistoryEnabled = enabled;
    }
}
