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
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.android_webview.R;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
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
    private static final String TRANSLATOR_PACKAGE_NAME = "com.samsung.android.app.interpreter";

    /**
     * Android Intent size limitations prevent sending over a megabyte of data. Limit query lengths
     * to 100kB because other things may be added to the Intent.
     */
    private static final int MAX_SHARE_QUERY_LENGTH = 100000;

    /**
     * On Samsung devices, OS mandates a different ordering than stock Android, and we want to be
     * consistent. This ordering is only used on WebView.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        SamsungDefaultItemOrder.CUT,
        SamsungDefaultItemOrder.COPY,
        SamsungDefaultItemOrder.PASTE,
        SamsungDefaultItemOrder.TRANSLATE,
        SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT,
        SamsungDefaultItemOrder.SELECT_ALL,
        SamsungDefaultItemOrder.SHARE,
        SamsungDefaultItemOrder.WEB_SEARCH
    })
    public @interface SamsungDefaultItemOrder {
        int CUT = 1;
        int COPY = 2;
        int PASTE = 3;
        int TRANSLATE = 4;
        int PASTE_AS_PLAIN_TEXT = 5;
        int SELECT_ALL = 6;
        int SHARE = 7;
        int WEB_SEARCH = 8;
    }

    public static void modifyDefaultMenuItems(
            List<SelectionMenuItem.Builder> menuItemBuilders,
            boolean isSelectionPassword,
            @NonNull String selectedText) {
        for (SelectionMenuItem.Builder builder : menuItemBuilders) {
            int menuItemOrder = getMenuItemOrder(builder.mId);
            if (menuItemOrder == -1) continue;
            builder.setOrderInCategory(menuItemOrder);
        }
        // TODO(crbug.com/41485684) Rewrite to have content APIs which support moving menu
        // items within groups instead of filtering our and re-adding.
        if (shouldAddTranslateMenu(selectedText, isSelectionPassword)) {
            // Get list of apps registered for text processing.
            List<ResolveInfo> textProcessActivities =
                    PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
            // Identify and get ResolveInfo for Translate app.
            ResolveInfo translateResolveInfo =
                    textProcessActivities.stream()
                            .filter(
                                    resolveInfo ->
                                            resolveInfo.activityInfo.packageName.equals(
                                                    TRANSLATOR_PACKAGE_NAME))
                            .findAny()
                            .orElse(null);
            if (translateResolveInfo == null) {
                // Do not add Translate menu if resolve info is not available.
                return;
            }
            // Create menu item from Translate app resolve info and then add to default menu.
            menuItemBuilders.add(
                    new SelectionMenuItem.Builder(
                                    translateResolveInfo.loadLabel(
                                            ContextUtils.getApplicationContext()
                                                    .getPackageManager()))
                            .setId(Menu.NONE)
                            .setIcon(null)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setOrderInCategory(SamsungDefaultItemOrder.TRANSLATE)
                            .setClickListener(
                                    getTranslationActionClickListener(
                                            selectedText, translateResolveInfo)));
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

    /**
     * Filters text processing activities. It should only be called if isManageAppsSupported returns
     * true.
     *
     * @param supportedActivities list of text processing activities to be filtered.
     * @return filtered list of text processing activities.
     */
    public static List<ResolveInfo> filterTextProcessingActivities(
            List<ResolveInfo> supportedActivities) {
        assert isManageAppsSupported();
        Context context = ContextUtils.getApplicationContext();
        String textManagerApps;
        try {
            // textManagerApps contains list of apps enabled in Manage apps fragment.
            // 1. A return value of null means user has not visited Manage apps fragment yet.
            // 2. A return value of empty means user has visited Manage apps fragment and has
            //    disabled all apps
            textManagerApps =
                    Settings.Global.getString(
                            context.getContentResolver(), STR_TEXT_MANAGER_APPS_RESOLVER);
        } catch (SecurityException e) {
            return supportedActivities;
        }
        if (textManagerApps == null || textManagerApps.isEmpty()) {
            // Return empty list if no text process menu item is enabled or user has never used
            // Manage apps, Settings#getString query on content resolver returns null in such case.
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

    private static boolean shouldAddTranslateMenu(
            @NonNull String selectedText, boolean isSelectionPassword) {
        return isManageAppsSupported()
                && PackageUtils.isPackageInstalled(TRANSLATOR_PACKAGE_NAME)
                && !selectedText.isEmpty()
                && !isSelectionPassword;
    }

    private static View.OnClickListener getTranslationActionClickListener(
            @NonNull String selectedText, ResolveInfo info) {
        return v -> {
            String textForProcessing =
                    (selectedText.length() >= MAX_SHARE_QUERY_LENGTH)
                            ? selectedText.substring(0, MAX_SHARE_QUERY_LENGTH) + "…"
                            : selectedText;
            Intent intent =
                    createProcessTextIntent()
                            .setClassName(info.activityInfo.packageName, info.activityInfo.name)
                            .putExtra(Intent.EXTRA_PROCESS_TEXT, textForProcessing);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                ContextUtils.getApplicationContext().startActivity(intent);
            } catch (android.content.ActivityNotFoundException ignored) {
            }
        };
    }

    private static Intent createProcessTextIntent() {
        return new Intent().setAction(Intent.ACTION_PROCESS_TEXT).setType("text/plain");
    }
}
