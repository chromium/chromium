// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.R;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The WebView delegate customizing text selection menu items in {@link SelectionPopupController}
 * for Android T and above in Samsung devices. It records WebView-specific histograms and ensures
 * order of Samsung-specific menu entries.
 */
@SuppressWarnings("DiscouragedApi")
@RequiresApi(Build.VERSION_CODES.TIRAMISU)
@NullMarked
public class SamsungSelectionActionMenuDelegate extends AutofillSelectionActionMenuDelegate {
    private static final ComponentName MANAGE_APPS_COMPONENT =
            new ComponentName(
                    "com.android.settings",
                    "com.samsung.android.settings.display.SecProcessTextManageAppsFragment");
    private static final String STR_TEXT_MANAGER_APPS_RESOLVER = "process_text_manager_apps";
    private static final String TRANSLATOR_PACKAGE_NAME = "com.samsung.android.app.interpreter";
    // Writing toolkit
    private static final String ACTION_WRITING_TOOLKIT =
            "com.samsung.android.intent.action.WritingToolkit";
    private static final String WRITING_TOOLKIT_ACTION_SHOW_HBD = "actionShowToolKitHbd";
    private static final String WRITING_TOOLKIT_SUBJECT = "toolkitSubject";
    private static final String WRITING_TOOLKIT_IS_TEXT_EDITABLE = "isTextEditable";
    private static final String WRITING_TOOLKIT_URI = "honeyboard://writing-toolkit";
    private static final int MAXIMUM_BUILD_VERSION_CODE_SUPPORTED = Build.VERSION_CODES.BAKLAVA;
    private static final int SCAN_TEXT_ID;
    private static final ComponentName WRITING_TOOLKIT_COMPONENT =
            new ComponentName(
                    "com.samsung.android.honeyboard",
                    "com.samsung.android.writingtoolkit.view.WritingToolkitActivity");
    private static final String FEATURE_FLAG_STATUS_CHECK_CLASS =
            "com.samsung.android.feature.SemFloatingFeature";
    private static final String AI_FEATURES_DISABLED_FLAG =
            "SEC_FLOATING_FEATURE_COMMON_DISABLE_NATIVE_AI";
    private static final String WRITING_TOOLKIT_SELECTED_TEXT = "selectedText";
    private static @Nullable Boolean sIsManageAppsSupported;
    private static @Nullable Boolean sAiFeaturesDisabled;

    /**
     * Android Intent size limitations prevent sending over a megabyte of data. Limit query lengths
     * to 100kB because other things may be added to the Intent.
     */
    private static final int MAX_SHARE_QUERY_LENGTH_BYTES = 100000;

    static {
        // We can not access this resource as regular resource since it is an extended resource.
        // Hence, we are using API that uses reflection.
        SCAN_TEXT_ID = Resources.getSystem().getIdentifier("writing_toolkit", "id", "android");
    }

    public SamsungSelectionActionMenuDelegate() {
        RecordHistogram.recordEnumeratedHistogram(
                AwSelectionActionMenuDelegate.TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                AwSelectionActionMenuDelegate.TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
                AwSelectionActionMenuDelegate.TextSelectionMenuOrdering.COUNT);
    }

    @Override
    public @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
        if (shouldUseSamsungMenuItemOrdering() && menuType == MenuType.FLOATING) {
            // Swap the ordering of SELECT_ALL and SHARE.
            return new @DefaultItem int[] {
                DefaultItem.CUT,
                DefaultItem.COPY,
                DefaultItem.PASTE,
                DefaultItem.PASTE_AS_PLAIN_TEXT,
                DefaultItem.SELECT_ALL,
                DefaultItem.SHARE,
                DefaultItem.WEB_SEARCH
            };
        }
        return super.getDefaultMenuItemOrder(menuType);
    }

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        ArrayList<SelectionMenuItem> additionalItems =
                new ArrayList<>(
                        super.getAdditionalMenuItems(
                                menuType, isSelectionPassword, isSelectionReadOnly, selectedText));
        if (menuType == MenuType.DROPDOWN) return additionalItems;
        if (isManageAppsSupported()) {
            additionalItems.add(
                    new SelectionMenuItem.Builder(
                                    org.chromium.android_webview.R.string.actionbar_manage_apps)
                            .setId(R.id.select_action_menu_manage_apps)
                            .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                            .setIcon(null)
                            .setOrderAndCategory(
                                    Menu.CATEGORY_SECONDARY,
                                    SelectionMenuItem.ItemGroupOffset.TEXT_PROCESSING_ITEMS)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setIntent(createManageAppsIntent())
                            .build());
        }
        if (shouldAddTranslateMenu(selectedText, isSelectionPassword)) {
            // Get list of apps registered for text processing.
            List<ResolveInfo> textProcessActivities =
                    PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
            // Identify and get ResolveInfo for Translate app.
            ResolveInfo translateResolveInfo = null;
            for (ResolveInfo resolveInfo : textProcessActivities) {
                if (resolveInfo.activityInfo.packageName.equals(TRANSLATOR_PACKAGE_NAME)) {
                    translateResolveInfo = resolveInfo;
                    break;
                }
            }
            if (translateResolveInfo == null) {
                // Do not add Translate menu if resolve info is not available.
                return additionalItems;
            }
            // Create menu item from Translate app resolve info and then add to default menu.
            additionalItems.add(
                    new SelectionMenuItem.Builder(
                                    translateResolveInfo.loadLabel(
                                            ContextUtils.getApplicationContext()
                                                    .getPackageManager()))
                            .setId(R.id.select_action_menu_translate)
                            .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                            .setIcon(null)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setOrderAndCategory(
                                    DefaultItem.PASTE, // Show after paste.
                                    SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS)
                            .setIntent(
                                    getTranslationActionIntent(selectedText, translateResolveInfo))
                            .build());
        }
        if (shouldAddWritingToolkitMenu(selectedText, isSelectionPassword)) {
            additionalItems.add(
                    new SelectionMenuItem.Builder(SCAN_TEXT_ID)
                            .setId(SCAN_TEXT_ID)
                            .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_ALWAYS)
                            .setOrderAndCategory(
                                    Menu.FIRST, // Show as first item after primary assist item.
                                    SelectionMenuItem.ItemGroupOffset.ASSIST_ITEMS)
                            .setIntent(
                                    createWritingToolkitIntent(selectedText, isSelectionReadOnly))
                            .build());
        }
        return additionalItems;
    }

    /**
     * Filters text processing activities. It should only be called if isManageAppsSupported returns
     * true.
     *
     * @param activities list of text processing activities to be filtered.
     * @return filtered list of text processing activities.
     */
    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities) {
        if (!isManageAppsSupported() || menuType == MenuType.DROPDOWN) {
            return activities;
        }
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
            return activities;
        }
        if (textManagerApps == null || textManagerApps.isEmpty()) {
            // Return empty list if no text process menu item is enabled or user has never used
            // Manage apps, Settings#getString query on content resolver returns null in such case.
            return new ArrayList<>();
        }
        List<String> splitTextManagerApps = Arrays.asList(textManagerApps.split("#"));
        List<ResolveInfo> updatedSupportedItems = new ArrayList<>();
        for (int i = 0; i < activities.size(); i++) {
            ResolveInfo resolveInfo = activities.get(i);
            if (resolveInfo.activityInfo == null
                    || (!splitTextManagerApps.contains(resolveInfo.activityInfo.packageName)
                            && !splitTextManagerApps.contains(resolveInfo.activityInfo.name))) {
                continue;
            }
            updatedSupportedItems.add(resolveInfo);
        }
        return updatedSupportedItems;
    }

    @Override
    public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
        return !isManageAppsSupported() || menuType == MenuType.DROPDOWN;
    }

    public static boolean shouldUseSamsungMenuItemOrdering() {
        return Build.VERSION.SDK_INT <= MAXIMUM_BUILD_VERSION_CODE_SUPPORTED && isSamsungDevice();
    }

    private static boolean isManageAppsSupported() {
        if (sIsManageAppsSupported != null) {
            return sIsManageAppsSupported;
        }
        if (!isSamsungDevice()
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                || Build.VERSION.SDK_INT > MAXIMUM_BUILD_VERSION_CODE_SUPPORTED) {
            sIsManageAppsSupported = false;
            return false;
        }
        Context context = ContextUtils.getApplicationContext();
        List<ResolveInfo> list =
                context.getPackageManager()
                        .queryIntentActivities(
                                createManageAppsIntent(), PackageManager.MATCH_DEFAULT_ONLY);
        sIsManageAppsSupported = !list.isEmpty();
        return sIsManageAppsSupported;
    }

    private static Intent createManageAppsIntent() {
        return new Intent().setComponent(MANAGE_APPS_COMPONENT);
    }

    private static boolean isSamsungDevice() {
        return "SAMSUNG".equalsIgnoreCase(Build.MANUFACTURER);
    }

    private static boolean shouldAddTranslateMenu(
            String selectedText, boolean isSelectionPassword) {
        return isManageAppsSupported()
                && PackageUtils.isPackageInstalled(TRANSLATOR_PACKAGE_NAME)
                && !selectedText.isEmpty()
                && !isSelectionPassword;
    }

    private static Intent getTranslationActionIntent(String selectedText, ResolveInfo info) {
        String textForProcessing =
                (selectedText.length() >= MAX_SHARE_QUERY_LENGTH_BYTES)
                        ? selectedText.substring(0, MAX_SHARE_QUERY_LENGTH_BYTES) + "…"
                        : selectedText;
        Intent intent =
                createProcessTextIntent()
                        .setClassName(info.activityInfo.packageName, info.activityInfo.name)
                        .putExtra(Intent.EXTRA_PROCESS_TEXT, textForProcessing);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    @Override
    public boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        if ((item.id == R.id.select_action_menu_translate
                        || item.id == R.id.select_action_menu_manage_apps)
                && item.intent != null) {
            startActivity(item.intent);
            return true;
        }
        if (isWritingToolKitMenuItem(item)) {
            SelectionPopupController selectionPopupController =
                    SelectionPopupController.fromWebContents(webContents);
            selectionPopupController.setPreserveSelectionOnNextLossOfFocus(true);
            Intent intent = item.intent;
            launchWritingToolkit(containerView, intent, selectionPopupController);
            return true;
        }

        // This may be an autofill menu item.
        return super.handleMenuItemClick(item, webContents, containerView);
    }

    private static boolean isWritingToolKitMenuItem(SelectionMenuItem item) {
        return SCAN_TEXT_ID != 0 && item.id == SCAN_TEXT_ID;
    }

    private static void launchWritingToolkit(
            @Nullable View containerView,
            @Nullable Intent intent,
            SelectionPopupController selectionPopupController) {
        Context context = ContextUtils.getApplicationContext();
        InputMethodManager inputMethodManager =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (inputMethodManager == null || intent == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            Bundle data = new Bundle();
            String selectedText = intent.getStringExtra(WRITING_TOOLKIT_SUBJECT);
            if (selectedText == null) selectedText = "";
            data.putString(WRITING_TOOLKIT_SELECTED_TEXT, selectedText);
            /**
             * sendAppPrivateCommand API handles Writing Toolkit for both editable and non-editable
             * fields from B-OS.
             */
            inputMethodManager.sendAppPrivateCommand(
                    containerView, WRITING_TOOLKIT_ACTION_SHOW_HBD, data);
            return;
        }
        inputMethodManager.sendAppPrivateCommand(
                containerView, WRITING_TOOLKIT_ACTION_SHOW_HBD, /* data= */ null);
        /**
         * sendAppPrivateCommand API handles Writing Toolkit for editable fields, eliminating the
         * need for startActivity.
         */
        if (selectionPopupController.isFocusedNodeEditable()) {
            return;
        }
        startActivity(intent);
    }

    private static Intent createWritingToolkitIntent(
            String selectedText, boolean isSelectionReadOnly) {
        String textForProcessing =
                (selectedText.length() >= MAX_SHARE_QUERY_LENGTH_BYTES)
                        ? selectedText.substring(0, MAX_SHARE_QUERY_LENGTH_BYTES) + "…"
                        : selectedText;
        Intent intent =
                new Intent()
                        .setAction(ACTION_WRITING_TOOLKIT)
                        .setData(Uri.parse(WRITING_TOOLKIT_URI))
                        .putExtra(WRITING_TOOLKIT_SUBJECT, textForProcessing)
                        .putExtra(WRITING_TOOLKIT_IS_TEXT_EDITABLE, !isSelectionReadOnly);
        intent.setFlags(0);
        return intent;
    }

    private static boolean shouldAddWritingToolkitMenu(
            String selectedText, boolean isSelectionPassword) {
        return isSamsungDevice()
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && Build.VERSION.SDK_INT <= MAXIMUM_BUILD_VERSION_CODE_SUPPORTED
                && !selectedText.isEmpty()
                && !isSelectionPassword
                && SCAN_TEXT_ID != 0
                && isWritingToolkitActivityAvailable()
                && !isAiFeaturesDisabled();
    }

    private static boolean isAiFeaturesDisabled() {
        if (sAiFeaturesDisabled != null) {
            return sAiFeaturesDisabled;
        }
        try {
            Class<?> featureStatusCheckClass = Class.forName(FEATURE_FLAG_STATUS_CHECK_CLASS);
            // Executing Reflection:
            // SemFloatingFeature.getInstance().getBoolean(AI_FEATURES_DISABLED_FLAG);
            // While invoking getInstance method argument is passed null since it is a
            // static method.
            sAiFeaturesDisabled =
                    (Boolean)
                            featureStatusCheckClass
                                    .getMethod("getBoolean", String.class)
                                    .invoke(
                                            featureStatusCheckClass
                                                    .getMethod("getInstance")
                                                    .invoke(/* obj= */ null),
                                            AI_FEATURES_DISABLED_FLAG);
        } catch (ClassNotFoundException
                | NoSuchMethodException
                | InvocationTargetException
                | IllegalAccessException e) {
            sAiFeaturesDisabled = true;
        }
        return Boolean.TRUE.equals(sAiFeaturesDisabled);
    }

    private static boolean isWritingToolkitActivityAvailable() {
        Intent intent = new Intent();
        intent.setComponent(WRITING_TOOLKIT_COMPONENT);
        return !PackageManagerUtils.queryIntentActivities(intent, PackageManager.MATCH_ALL)
                .isEmpty();
    }

    private static Intent createProcessTextIntent() {
        return new Intent().setAction(Intent.ACTION_PROCESS_TEXT).setType("text/plain");
    }

    /**
     * Used to start activity when we do not handle any result. If we need to handle result as a
     * response to the activity launch, please use {@link AwContents#startActivityForResult(Intent,
     * int)}. Result from startActivityForResult are mostly used in case when selection is in
     * editable fields in our use cases. Otherwise, startActivity and startActivityForResult has
     * same effect.
     *
     * @param intent Intent used to start activity.
     */
    private static void startActivity(Intent intent) {
        try {
            ContextUtils.getApplicationContext().startActivity(intent);
        } catch (ActivityNotFoundException ignored) {
        }
    }
}
