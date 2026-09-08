// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browserservices.TwaValidator;
import org.chromium.chrome.browser.open_in_app.OpenInAppDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.InstallTrigger;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerProvider;
import org.chromium.components.webapps.pwa_universal_install.PwaUniversalInstallBottomSheetCoordinator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/**
 * Handles WebAPK and PWA install actions from the app menu.
 *
 * <p>Provides entry points for Universal Install, Add to Homescreen, and opening an already
 * installed WebAPK without depending on ChromeActivity.
 */
@NullMarked
public class AppInstallMenuHandler {
    private static final String TAG = "AppInstallMenu";

    /**
     * Shows the Universal Install bottom sheet or falls back to Add To Homescreen.
     *
     * <p>If the bottom sheet controller is unavailable, this method directly triggers the Install
     * App flow to match the menu item that would have been shown when Universal Install is
     * disabled.
     *
     * @param activity The hosting activity used for UI and intents.
     * @param windowAndroid Window context for bottom sheet presentation.
     * @param currentTab The tab from which installation is triggered.
     * @return True when an install flow was started.
     */
    public static boolean doUniversalInstall(
            Activity activity,
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            Tab currentTab) {
        @Nullable WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;

        String manifestId = WebappRegistry.getManifestIdOrUrl(currentTab);

        boolean isPending = WebappRegistry.getInstance().isWebApkPending(manifestId);
        if (isPending) {
            String appName = currentTab.getTitle();
            android.widget.Toast.makeText(
                            activity,
                            activity.getString(
                                    R.string.notification_webapk_install_in_progress, appName),
                            android.widget.Toast.LENGTH_SHORT)
                    .show();
            return true;
        }

        if (WebappRegistry.getInstance().wasWebApkRecentlyInstalled(manifestId, 10000)) {
            return doOpenWebApp(activity, currentTab);
        }

        @Nullable BottomSheetController controller =
                BottomSheetControllerProvider.from(windowAndroid);
        if (controller == null) {
            // We have three options when this function fails. One is to abort the operation and do
            // nothing (by returning false), or we can make one of the two options of the Universal
            // Install dialog the default and go with that in case of errors. Since Install App is
            // the menu item that would have been shown, if Universal Install was disabled, we
            // fall back to the Install App option.
            return doAddToHomescreen(
                    activity,
                    windowAndroid,
                    modalDialogManager,
                    currentTab,
                    AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
        }

        boolean webAppInstalled =
                WebappRegistry.getInstance().isAppInstalledForUrl(currentTab.getUrl());

        PwaUniversalInstallBottomSheetCoordinator pwaUniversalInstallBottomSheetCoordinator =
                new PwaUniversalInstallBottomSheetCoordinator(
                        activity,
                        webContents,
                        () -> {
                            doAddToHomescreen(
                                    activity,
                                    windowAndroid,
                                    modalDialogManager,
                                    currentTab,
                                    AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
                        },
                        () -> {
                            doAddToHomescreen(
                                    activity,
                                    windowAndroid,
                                    modalDialogManager,
                                    currentTab,
                                    AppMenuVerbiage.APP_MENU_OPTION_ADD_TO_HOMESCREEN);
                        },
                        () -> {
                            doOpenWebApp(activity, currentTab);
                        },
                        webAppInstalled,
                        controller,
                        R.drawable.outline_chevron_right_24dp,
                        R.drawable.down_arrow_on_circular_background,
                        R.drawable.chrome_logo_on_circular_background);
        pwaUniversalInstallBottomSheetCoordinator.showBottomSheetAsync();
        return true;
    }

    /**
     * Triggers the Add To Homescreen or Install App flow.
     *
     * <p>When the menu item represents Install App, this method first attempts to expand the PWA
     * bottom sheet installer. Otherwise it falls back to the standard Add To Homescreen
     * coordinator.
     *
     * @param activity The hosting activity.
     * @param windowAndroid Window context.
     * @param modalDialogManager Manager for confirmation dialogs, may be null.
     * @param currentTab The tab being installed.
     * @param menuItemType Menu action type from {@link AppMenuVerbiage}.
     * @return True when an installation UI was shown.
     */
    public static boolean doAddToHomescreen(
            Activity activity,
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            Tab currentTab,
            int menuItemType) {
        @Nullable WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (menuItemType == AppMenuVerbiage.APP_MENU_OPTION_INSTALL) {
            @Nullable PwaBottomSheetController controller =
                    PwaBottomSheetControllerProvider.from(windowAndroid);
            if (controller != null
                    && controller.requestOrExpandBottomSheetInstaller(
                            webContents, InstallTrigger.MENU)) {
                return true;
            }
        }

        AddToHomescreenCoordinator.showForAppMenu(
                activity, windowAndroid, modalDialogManager, webContents, menuItemType);
        return true;
    }

    /**
     * Launches an installed web app for the current URL.
     *
     * <p>Queries the system for a matching web app package and starts it. Displays a toast when no
     * handler is available.
     *
     * @param activity The activity used to start the intent.
     * @param currentTab The tab whose URL should be opened.
     * @return True after the launch attempt.
     */
    public static boolean doOpenWebApp(Activity activity, Tab currentTab) {
        OpenInAppDelegate delegate = OpenInAppDelegate.from(currentTab);
        if (delegate != null) {
            OpenInAppDelegate.OpenInAppInfo openInAppInfo = delegate.getCurrentOpenInAppInfo();
            if (openInAppInfo != null && openInAppInfo.action != null) {
                openInAppInfo.action.run();
                return true;
            }
        }

        Context context = ContextUtils.getApplicationContext();
        @Nullable String packageName = null;
        @Nullable WebContents webContents = currentTab.getWebContents();
        if (webContents != null) {
            String manifestId = WebappRegistry.getManifestIdOrUrl(currentTab);
            packageName = WebappRegistry.getInstance().findWebApkWithManifestId(manifestId);
        }

        String currentTabUrl = currentTab.getUrl().getSpec();
        if (packageName == null) {
            packageName = WebApkValidator.queryFirstWebApkPackage(context, currentTabUrl);
        }

        // Try launching a WebAPK first.
        if (packageName != null) {
            Intent launchIntent =
                    WebApkNavigationClient.createLaunchWebApkIntent(
                            packageName, currentTabUrl, false);
            try {
                context.startActivity(launchIntent);
            } catch (ActivityNotFoundException | SecurityException e) {
                Log.w(TAG, "Failed to launch WebAPK: " + packageName, e);
                Toast.makeText(context, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
            }
            return true;
        }

        // Fallback to launching a TWA instead if a WebApk is not found.
        String twaPackage = TwaValidator.queryFirstTwaPackage(currentTab.getUrl());
        if (twaPackage != null) {
            Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(currentTabUrl));
            launchIntent.setPackage(twaPackage);
            launchIntent.addCategory(Intent.CATEGORY_BROWSABLE);
            launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                context.startActivity(launchIntent);
            } catch (ActivityNotFoundException | SecurityException e) {
                Log.w(TAG, "Failed to launch TWA: " + twaPackage, e);
                Toast.makeText(context, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
            }
            return true;
        }

        // Neither a WebApk or a TWA was found.
        Toast.makeText(context, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
        return true;
    }
}
