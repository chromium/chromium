// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.chromium.android_webview.R;
import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.common.ContentFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Helper class for customizing text selection menu items in {@link SelectionPopupController} for
 * Samsung devices.
 */
public class SamsungSelectionActionMenuHelper {
    private static final ComponentName MANAGE_APPS_COMPONENT =
            new ComponentName(
                    "com.android.settings",
                    "com.samsung.android.settings.display.SecProcessTextManageAppsFragment");
    private static final String STR_TEXT_MANAGER_APPS_RESOLVER = "process_text_manager_apps";

    /**
     * On Samsung devices, OS mandates a different ordering than stock Android, and we want to be
     * consistent. This ordering is only used on WebView.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        SamsungDefaultItemOrder.CUT,
        SamsungDefaultItemOrder.COPY,
        SamsungDefaultItemOrder.PASTE,
        SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT,
        SamsungDefaultItemOrder.SELECT_ALL,
        SamsungDefaultItemOrder.SHARE,
        SamsungDefaultItemOrder.WEB_SEARCH
    })
    public @interface SamsungDefaultItemOrder {
        int CUT = 1;
        int COPY = 2;
        int PASTE = 3;
        int PASTE_AS_PLAIN_TEXT = 4;
        int SELECT_ALL = 5;
        int SHARE = 6;
        int WEB_SEARCH = 7;
    }

    public static void modifyDefaultMenuItems(List<SelectionMenuItem.Builder> menuItemBuilders) {
        for (SelectionMenuItem.Builder builder : menuItemBuilders) {
            int menuItemOrder = getMenuItemOrder(builder.mId);
            if (menuItemOrder == -1) continue;
            builder.setOrderInCategory(menuItemOrder);
        }
    }

    public static List<SelectionMenuItem> getAdditionalTextProcessingItems() {
        List<SelectionMenuItem> additionalItemBuilderList = new ArrayList<>();
        additionalItemBuilderList.add(
                new SelectionMenuItem.Builder(R.string.actionbar_manage_apps)
                        .setId(Menu.NONE)
                        .setIcon(null)
                        .setOrderInCategory(Menu.CATEGORY_SECONDARY)
                        .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                        .setClickListener(null)
                        .setIntent(createManageAppsIntent())
                        .build());
        return additionalItemBuilderList;
    }

    public static List<ResolveInfo> filterTextProcessingActivities(
            List<ResolveInfo> supportedActivities) {
        Context context = ContextUtils.getApplicationContext();
        String textManagerApps;
        try {
            textManagerApps =
                    Settings.Global.getString(
                            context.getContentResolver(), STR_TEXT_MANAGER_APPS_RESOLVER);
        } catch (SecurityException e) {
            return supportedActivities;
        }
        if (textManagerApps == null) {
            // Do not modify list if returned string is null. Query can return null
            // if there is no value for the key. As per current implementation, value is
            // inserted initially when preference fragment is launched at least once.
            return supportedActivities;
        }
        if (textManagerApps.isEmpty()) {
            // Return empty list as no text process menu item is enabled.
            return new ArrayList<>();
        }
        List<String> splitTextManagerApps = Arrays.asList(textManagerApps.split("#"));
        List<ResolveInfo> updatedSupportedItems = new ArrayList<>();
        for (int i = 0; i < supportedActivities.size(); i++) {
            ResolveInfo resolveInfo = supportedActivities.get(i);
            if (resolveInfo.activityInfo == null
                    || !splitTextManagerApps.contains(resolveInfo.activityInfo.packageName)) {
                continue;
            }
            updatedSupportedItems.add(resolveInfo);
        }
        return updatedSupportedItems;
    }

    public static boolean shouldUseSamsungMenuItemOrdering() {
        return Build.VERSION.SDK_INT <= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && isSamsungDevice()
                && ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION);
    }

    private static int getMenuItemOrder(@IdRes int id) {
        if (id == R.id.select_action_menu_cut) {
            return SamsungDefaultItemOrder.CUT;
        } else if (id == R.id.select_action_menu_copy) {
            return SamsungDefaultItemOrder.COPY;
        } else if (id == R.id.select_action_menu_paste) {
            return SamsungDefaultItemOrder.PASTE;
        } else if (id == R.id.select_action_menu_select_all) {
            return SamsungDefaultItemOrder.SELECT_ALL;
        } else if (id == R.id.select_action_menu_share) {
            return SamsungDefaultItemOrder.SHARE;
        } else if (id == R.id.select_action_menu_paste_as_plain_text) {
            return SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT;
        } else if (id == R.id.select_action_menu_web_search) {
            return SamsungDefaultItemOrder.WEB_SEARCH;
        }
        return -1;
    }

    public static boolean isManageAppsSupported() {
        if (!isSamsungDevice()
                || Build.VERSION.SDK_INT != Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                || !ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION)) {
            return false;
        }
        Context context = ContextUtils.getApplicationContext();
        List<ResolveInfo> list =
                context.getPackageManager()
                        .queryIntentActivities(
                                createManageAppsIntent(), PackageManager.MATCH_DEFAULT_ONLY);
        return list.size() > 0;
    }

    private static Intent createManageAppsIntent() {
        return new Intent().setComponent(MANAGE_APPS_COMPONENT);
    }

    private static boolean isSamsungDevice() {
        return "SAMSUNG".equalsIgnoreCase(Build.MANUFACTURER);
    }
}
