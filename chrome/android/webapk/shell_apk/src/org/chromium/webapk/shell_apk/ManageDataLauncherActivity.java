// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.Toast;

import androidx.annotation.RequiresApi;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.Collections;
import java.util.List;

/**
 * Handles site settings shortcuts for WebApks. The shortcut opens the web browser's site settings
 * for the start url associated with the WebApk.
 */
public class ManageDataLauncherActivity extends Activity {
    public static final String ACTION_SITE_SETTINGS =
            "android.support.customtabs.action.ACTION_MANAGE_TRUSTED_WEB_ACTIVITY_DATA";

    public static final String SITE_SETTINGS_SHORTCUT_ID =
            "android.support.customtabs.action.SITE_SETTINGS_SHORTCUT";

    private static final String EXTRA_SITE_SETTINGS_URL = "SITE_SETTINGS_URL";
    private static final String EXTRA_PROVIDER_PACKAGE = "PROVIDER_PACKAGE";

    private static final String CATEGORY_LAUNCH_WEBAPK_SITE_SETTINGS =
            "androidx.browser.trusted.category.LaunchWebApkSiteSettings";

    private static final String ACTION_CUSTOM_TABS_CONNECTION =
            "android.support.customtabs.action.CustomTabsService";

    private String mProviderPackage;

    /**
     * The url of the page for which the settings will be shown. Must be provided as an intent extra
     * to {@link ManageDataLauncherActivity}.
     */
    private Uri mUrl;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mProviderPackage = getIntent().getStringExtra(EXTRA_PROVIDER_PACKAGE);
        mUrl = Uri.parse(getIntent().getStringExtra(EXTRA_SITE_SETTINGS_URL));

        if (!siteSettingsShortcutEnabled(this, mProviderPackage)) {
            handleNoSupportForLaunchSettings();
            return;
        }
        setContentView(createLoadingView());
        launchSettings();
    }

    /** Returns a view with a loading spinner. */
    private View createLoadingView() {
        ProgressBar progressBar = new ProgressBar(this);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        progressBar.setLayoutParams(params);
        FrameLayout layout = new FrameLayout(this);
        layout.addView(progressBar);
        return layout;
    }

    /**
     * Called if a provider doesn't support the launch settings feature. Shows a toast telling the
     * user how to fix it, then finishes the activity.
     */
    private void handleNoSupportForLaunchSettings() {
        String appName;
        try {
            ApplicationInfo info = getPackageManager().getApplicationInfo(mProviderPackage, 0);
            appName = getPackageManager().getApplicationLabel(info).toString();
        } catch (PackageManager.NameNotFoundException e) {
            appName = mProviderPackage;
        }

        Toast.makeText(
                        this,
                        getString(R.string.no_support_for_launch_settings, appName),
                        Toast.LENGTH_LONG)
                .show();
        finish();
    }

    @Override
    protected void onStop() {
        super.onStop();
        finish();
    }

    private void launchSettings() {
        Intent intent = new Intent();
        intent.setAction(ACTION_SITE_SETTINGS);
        intent.setPackage(mProviderPackage);
        intent.setData(mUrl);
        intent.putExtra(WebApkConstants.EXTRA_IS_WEBAPK, true);

        try {
            startActivityForResult(intent, /* requestCode= */ 0);
            finish();
        } catch (ActivityNotFoundException e) {
            handleNoSupportForLaunchSettings();
        }
    }

    private static boolean siteSettingsShortcutEnabled(Context context, String providerPackage) {
        Bundle metadata = WebApkUtils.readMetaData(context);
        if (metadata == null
                || !metadata.getBoolean(WebApkMetaDataKeys.ENABLE_SITE_SETTINGS_SHORTCUT, false)) {
            return false;
        }

        Intent intent = new Intent(ACTION_CUSTOM_TABS_CONNECTION);
        intent.addCategory(CATEGORY_LAUNCH_WEBAPK_SITE_SETTINGS);
        intent.setPackage(providerPackage);
        List<ResolveInfo> services =
                context.getPackageManager()
                        .queryIntentServices(intent, PackageManager.GET_RESOLVED_FILTER);
        return services.size() > 0;
    }

    /**
     * Returns the {@link ShortcutInfo} for a dynamic shortcut into site settings, provided that
     * {@link ManageDataLauncherActivity} is present in the manifest and an Intent for managing site
     * settings is available.
     *
     * <p>Otherwise returns null if {@link ManageDataLauncherActivity} is not launchable or if
     * shortcuts are not supported by the Android SDK version.
     *
     * <p>The shortcut returned does not specify an activity. Thus when the shortcut is added, the
     * app's main activity will be used by default. This activity needs to define the MAIN action
     * and LAUNCHER category in order to attach the shortcut.
     */
    @RequiresApi(Build.VERSION_CODES.N_MR1)
    private static ShortcutInfo createSiteSettingsShortcutInfo(
            Context context, String url, String providerPackage) {
        Intent siteSettingsIntent = new Intent(context, ManageDataLauncherActivity.class);
        // Intent needs to have an action set, we can set an arbitrary action.
        siteSettingsIntent.setAction(ACTION_SITE_SETTINGS);
        siteSettingsIntent.putExtra(EXTRA_SITE_SETTINGS_URL, url);
        siteSettingsIntent.putExtra(EXTRA_PROVIDER_PACKAGE, providerPackage);

        return new ShortcutInfo.Builder(context, SITE_SETTINGS_SHORTCUT_ID)
                .setShortLabel(context.getString(R.string.site_settings_short_label))
                .setLongLabel(context.getString(R.string.site_settings_long_label))
                .setIcon(Icon.createWithResource(context, R.drawable.ic_site_settings))
                .setIntent(siteSettingsIntent)
                .build();
    }

    /**
     * Adds dynamic shortcut to site settings if the provider and android version support it.
     *
     * <p>Removes previously added site settings shortcut if it is no longer supported, e.g. the
     * user changed their default browser.
     */
    public static void updateSiteSettingsShortcut(
            Context context, HostBrowserLauncherParams params) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1) return;

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);

        // Remove potentially existing shortcut if package does not support shortcuts.
        if (!siteSettingsShortcutEnabled(context, params.getHostBrowserPackageName())) {
            shortcutManager.removeDynamicShortcuts(
                    Collections.singletonList(
                            ManageDataLauncherActivity.SITE_SETTINGS_SHORTCUT_ID));
            return;
        }

        ShortcutInfo shortcut =
                createSiteSettingsShortcutInfo(
                        context, params.getStartUrl(), params.getHostBrowserPackageName());
        shortcutManager.addDynamicShortcuts(Collections.singletonList(shortcut));
    }
}
