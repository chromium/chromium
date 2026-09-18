// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.res.Configuration;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;

/** Utility class for checking if Settings in Tab feature is enabled. */
@NullMarked
public class SettingsInTab {
    /**
     * Returns whether the Settings in Tab feature flags are enabled, without considering the device
     * form factor or the screen size.
     *
     * <p>Use this to decide whether settings may be hosted in a tab at all, for example when
     * recreating an existing settings tab. Form factor must not be considered there, because the
     * screen width can change while settings is open (e.g. folding a foldable device or changing
     * the display density on automotive) and an open settings tab must keep working. See
     * crbug.com/562619494.
     *
     * <p>To decide whether to <em>open</em> settings in a tab, use {@link
     * #shouldOpenSettingsInTab()} instead. To find out how an already-open settings UI is hosted,
     * use {@link SettingsHost#isShownInTab()}, usually via {@link SettingsHostUtil}.
     */
    public static boolean isFeatureEnabled() {
        // SettingsInTab requires SettingsMultiColumn, which is disabled by some tests.
        if (!ChromeFeatureList.sSettingsMultiColumn.isEnabled()) return false;

        // Desktop uses SettingsInTabDesktop; tablet uses SettingsInTab.
        return DeviceInfo.isDesktop()
                ? ChromeFeatureList.sSettingsInTabDesktop.isEnabled()
                : ChromeFeatureList.sSettingsInTab.isEnabled();
    }

    /**
     * Returns whether opening settings should create a settings tab rather than starting {@code
     * SettingsActivity}. True if the feature flag is enabled and the device form factor is
     * tablet/desktop.
     *
     * <p>This depends on the current screen width, so its value can change over the life of the
     * process. It must therefore only be called when settings is opened, and the result must be
     * remembered by the resulting settings UI. To find out how an already-open settings UI is
     * hosted, use {@link SettingsHost#isShownInTab()}, usually via {@link SettingsHostUtil}.
     */
    public static boolean shouldOpenSettingsInTab() {
        if (!isFeatureEnabled()) return false;

        // DeviceInfo.isDesktop() is checked in addition to isNonMultiDisplayContextOnTablet()
        // because desktop windows can be resized to narrow widths (< 600dp).
        if (DeviceInfo.isDesktop()) return true;

        // Use an Activity context when available because theme changes reset application-level
        // resource configurations, causing getApplicationContext() to lose its tablet screen width
        // qualifiers (-sw600dp).
        Context context = ApplicationStatus.getLastTrackedFocusedActivity();
        if (context == null) {
            context = ContextUtils.getApplicationContext();
            // Automotive activities scale up UI density (see ChromeBaseAppCompatActivity),
            // which reduces smallestScreenWidthDp. Apply automotive scaling to the fallback
            // application context so the tablet check matches what activities will experience.
            if (DeviceInfo.isAutomotive()) {
                Configuration config = new Configuration();
                DisplayUtil.scaleUpConfigurationForAutomotive(context, config);
                context = context.createConfigurationContext(config);
            }
        }
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }
}
