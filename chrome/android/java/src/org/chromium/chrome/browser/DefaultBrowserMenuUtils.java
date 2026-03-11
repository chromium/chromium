// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.DefaultBrowserInfo;

/** A utility class for querying information about the default browser setting. */
@NullMarked
public final class DefaultBrowserMenuUtils {

    /**
     * Returns the title for the menu item that opens a link in the default browser.
     *
     * <p>This method retrieves the appropriate menu title string based on whether Chrome is forced
     * as the default browser or if the system's actual default browser should be used.
     *
     * <p>The returned title will be one of:
     *
     * <ul>
     *   <li>The current application's package label (e.g., "Open in Chrome") if {@code
     *       forceChromeAsDefault} is true
     *   <li>The system default browser's package label (e.g., "Open in Firefox") if {@code
     *       forceChromeAsDefault} is false and a default browser is set
     *   <li>A generic "Open in default browser" string if no default browser is configured or the
     *       package label cannot be determined
     * </ul>
     */
    public static String getTitleOpenInDefaultBrowser(boolean forceChromeAsDefault) {
        if (forceChromeAsDefault) {
            return getTitleFromPackageLabel(
                    ContextUtils.getApplicationContext(), ApkInfo.getHostPackageLabel());
        } else {
            ResolveInfo info = DefaultBrowserInfo.getDefaultWebBrowserInfo();
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            CharSequence infoLabel = info != null && info.match != 0 ? info.loadLabel(pm) : null;
            String packageLabel = infoLabel != null ? infoLabel.toString() : null;
            return getTitleFromPackageLabel(ContextUtils.getApplicationContext(), packageLabel);
        }
    }

    private static String getTitleFromPackageLabel(Context context, @Nullable String packageLabel) {
        return packageLabel == null
                ? context.getString(R.string.menu_open_in_product_default)
                : context.getString(R.string.menu_open_in_product, packageLabel);
    }
}
