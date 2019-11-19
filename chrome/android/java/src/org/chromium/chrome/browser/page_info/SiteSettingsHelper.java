// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.net.GURLUtils;

/**
 * This class contains helper methods for determining site settings availability and showing the
 * site settings page.
 */
public class SiteSettingsHelper {
    /**
     * Whether site settings is available for a given {@link Tab}.
     */
    public static boolean isSiteSettingsAvailable(Tab tab) {
        boolean isOfflinePage = OfflinePageUtils.getOfflinePage(tab) != null;
        boolean isPreviewPage =
                PreviewsAndroidBridge.getInstance().shouldShowPreviewUI(tab.getWebContents());
        String scheme = GURLUtils.getScheme(tab.getOriginalUrl());
        return !isOfflinePage && !isPreviewPage
                && (UrlConstants.HTTP_SCHEME.equals(scheme)
                        || UrlConstants.HTTPS_SCHEME.equals(scheme));
    }

    /**
     * Shows the site settings activity for a given url.
     */
    public static void showSiteSettings(Context context, String fullUrl) {
        Intent preferencesIntent = PreferencesLauncher.createIntentForSettingsPage(context,
                SingleWebsitePreferences.class.getName(),
                SingleWebsitePreferences.createFragmentArgsForSite(fullUrl));
        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            context.startActivity(preferencesIntent);
        }
    }
}
