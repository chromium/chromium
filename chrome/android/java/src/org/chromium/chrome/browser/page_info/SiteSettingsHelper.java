// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * This class contains helper methods for determining site settings availability and showing the
 * site settings page.
 */
public class SiteSettingsHelper {
    /**
     * Whether site settings is available for a given {@link WebContents}.
     * @param webContents The WebContents for which to check the site settings.
     */
    public static boolean isSiteSettingsAvailable(WebContents webContents) {
        boolean isOfflinePage = OfflinePageUtils.getOfflinePage(webContents) != null;
        // TODO(crbug.com/1033178): dedupe the DomDistillerUrlUtils#getOriginalUrlFromDistillerUrl()
        // calls.
        GURL url = webContents != null
                ? DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(webContents.getVisibleUrl())
                : null;
        return !isOfflinePage && url != null && UrlUtilities.isHttpOrHttps(url);
    }

    /**
     * Show the single category settings page for given category and type.
     */
    public static void showCategorySettings(
            Context context, @SiteSettingsCategory.Type int category) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Bundle extras = new Bundle();
        extras.putString(SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(category));
        extras.putString(SingleCategorySettings.EXTRA_TITLE,
                context.getResources().getString(
                        ContentSettingsResources.getTitleForCategory(category,
                                new ChromeSiteSettingsDelegate(
                                        context, Profile.getLastUsedRegularProfile()))));
        Intent preferencesIntent = settingsLauncher.createSettingsActivityIntent(
                context, SingleCategorySettings.class.getName(), extras);
        launchIntent(context, preferencesIntent);
    }

    private static void launchIntent(Context context, Intent intent) {
        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            context.startActivity(intent);
        }
    }
}
