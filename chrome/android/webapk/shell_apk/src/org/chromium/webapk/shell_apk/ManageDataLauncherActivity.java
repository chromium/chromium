// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;

import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.Collections;
import java.util.List;

/**
 * A convenience class for adding site setting shortcuts into WebApks.
 * The shortcut opens the web browser's site settings for the url
 * associated to the WebApk.
 */
public class ManageDataLauncherActivity extends Activity {
    private static final String TAG = "ManageDataLauncher";

    public static final String ACTION_SITE_SETTINGS =
            "android.support.customtabs.action.ACTION_MANAGE_TRUSTED_WEB_ACTIVITY_DATA";

    public static final String SITE_SETTINGS_SHORTCUT_ID =
            "android.support.customtabs.action.SITE_SETTINGS_SHORTCUT";

    private static final String EXTRA_SITE_SETTINGS_URL = "SITE_SETTINGS_URL";
    private static final String EXTRA_PROVIDER_PACKAGE = "PROVIDER_PACKAGE";

    private static final String CATEGORY_LAUNCH_WEBAPK_SITE_SETTINGS =
            "androidx.browser.trusted.category.LaunchWebApkSiteSettings";

    @Nullable
    private String mProviderPackage;

    @Nullable
    private CustomTabsServiceConnection mConnection;

    @Nullable
    private Uri mUrl;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mProviderPackage = getIntent().getStringExtra(EXTRA_PROVIDER_PACKAGE);
        mUrl = Uri.parse(getIntent().getStringExtra(EXTRA_SITE_SETTINGS_URL));

        if (!supportsWebApkManageSpace(this, mProviderPackage)) {
            handleNoSupportForManageSpace();
            return;
        }
        setContentView(createLoadingView());

        mConnection = new CustomTabsServiceConnection() {
            @Override
            public void onCustomTabsServiceConnected(
                    ComponentName componentName, CustomTabsClient client) {
                if (!isFinishing()) {
                    launchSettings(client.newSession(null));
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName componentName) {}
        };
        CustomTabsClient.bindCustomTabsService(this, mProviderPackage, mConnection);
    }

    /**
     * Returns the url of the page for which the settings will be shown.
     * The url must be provided as an intent extra to {@link ManageDataLauncherActivity}.
     */
    @Nullable
    private Uri getWebApkStartUrl() {
        return mUrl;
    }

    /**
     * Returns a view with a loading spinner.
     */
    @NonNull
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
     * Called if a TWA provider doesn't support manage space feature. The default behavior is to
     * show a toast telling the user where the data is stored.
     */
    private void handleNoSupportForManageSpace() {
        String appName;
        try {
            ApplicationInfo info = getPackageManager().getApplicationInfo(mProviderPackage, 0);
            appName = getPackageManager().getApplicationLabel(info).toString();
        } catch (PackageManager.NameNotFoundException e) {
            appName = mProviderPackage;
        }

        Toast.makeText(this, getString(R.string.no_support_for_manage_space, appName),
                     Toast.LENGTH_LONG)
                .show();
        finish();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (mConnection != null) {
            unbindService(mConnection);
        }
        finish();
    }

    private void launchSettings(CustomTabsSession session) {
        boolean success =
                launchBrowserSiteSettings(this, session, mProviderPackage, getWebApkStartUrl());
        if (success) {
            finish();
        } else {
            handleNoSupportForManageSpace();
        }
    }

    private static boolean launchBrowserSiteSettings(
            Activity activity, CustomTabsSession session, String packageName, Uri defaultUri) {
        // A Custom Tabs Session is required so that the browser can verify this app's identity.
        Intent intent = new CustomTabsIntent.Builder().setSession(session).build().intent;
        intent.setAction(ACTION_SITE_SETTINGS);
        intent.setPackage(packageName);
        intent.setData(defaultUri);
        intent.putExtra(WebApkConstants.EXTRA_IS_WEBAPK, true);
        try {
            activity.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
            return false;
        }
    }

    private static boolean supportsWebApkManageSpace(Context context, String providerPackage) {
        Intent customTabsIntent = new Intent(CustomTabsService.ACTION_CUSTOM_TABS_CONNECTION);
        customTabsIntent.addCategory(CATEGORY_LAUNCH_WEBAPK_SITE_SETTINGS);
        customTabsIntent.setPackage(providerPackage);
        List<ResolveInfo> services = context.getPackageManager().queryIntentServices(
                customTabsIntent, PackageManager.GET_RESOLVED_FILTER);
        return services.size() > 0;
    }

    /**
     * Returns the {@link ShortcutInfo} for a dynamic shortcut into site settings,
     * provided that {@link ManageDataLauncherActivity} is present in the manifest
     * and an Intent for managing site settings is available.
     *
     * Otherwise returns null if {@link ManageDataLauncherActivity} is not launchable
     * or if shortcuts are not supported by the Android SDK version.
     *
     * The shortcut returned does not specify an activity. Thus when the shortcut is added,
     * the app's main activity will be used by default. This activity needs to define the
     * MAIN action and LAUNCHER category in order to attach the shortcut.
     */
    @NonNull
    @TargetApi(Build.VERSION_CODES.N_MR1)
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
     * Adds dynamic shortcut to site settings if the twa provider and android version supports it.
     *
     * Removes previously added site settings shortcut if it is no longer supported, e.g. the user
     * changed their default browser.
     */
    public static void updateSiteSettingsShortcut(
            Context context, HostBrowserLauncherParams params) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1) return;

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);

        // Remove potentially existing shortcut if package does not support shortcuts.
        if (!supportsWebApkManageSpace(context, params.getHostBrowserPackageName())) {
            shortcutManager.removeDynamicShortcuts(Collections.singletonList(
                    ManageDataLauncherActivity.SITE_SETTINGS_SHORTCUT_ID));
            return;
        }

        ShortcutInfo shortcut = createSiteSettingsShortcutInfo(
                context, params.getStartUrl(), params.getHostBrowserPackageName());
        shortcutManager.addDynamicShortcuts(Collections.singletonList(shortcut));
    }
}
