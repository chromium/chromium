// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
    private static final String WRITING_TOOLKIT_HBD = "actionShowToolKitHbd";
    private static final String WRITING_TOOLKIT_SUBJECT = "toolkitSubject";
    private static final String WRITING_TOOLKIT_IS_TEXT_EDITABLE = "isTextEditable";
    private static final String WRITING_TOOLKIT_URI = "honeyboard://writing-toolkit";
    public static final int WRITING_TOOLKIT_REQUEST_CODE = 102;
    private static final String TEXT_KEY_FROM_WRITING_TOOLKIT = "toolkitText";
    private static final String ACTION_KEY_FROM_WRITING_TOOLKIT = "toolkitAction";
    private static final String REPLACE_ACTION_KEY_FROM_WRITING_TOOLKIT = "replace";
    private static final int SCAN_TEXT_ID;
    private static final ComponentName WRITING_TOOLKIT_COMPONENT =
            new ComponentName(
                    "com.samsung.android.honeyboard",
                    "com.samsung.android.writingtoolkit.view.WritingToolkitActivity");
    private static Boolean sIsManageAppsSupported;

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

    /**
     * On Samsung devices, OS mandates a different ordering than stock Android, and we want to be
     * consistent. This ordering is only used on WebView.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        SamsungDefaultItemOrder.WRITING_TOOLKIT,
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
        int WRITING_TOOLKIT = 1;
        int CUT = 2;
        int COPY = 3;
        int PASTE = 4;
        int TRANSLATE = 5;
        int PASTE_AS_PLAIN_TEXT = 6;
        int SELECT_ALL = 7;
        int SHARE = 8;
        int WEB_SEARCH = 9;
    }

    public SamsungSelectionActionMenuDelegate() {
        RecordHistogram.recordEnumeratedHistogram(
                AwSelectionActionMenuDelegate.TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                AwSelectionActionMenuDelegate.TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
                AwSelectionActionMenuDelegate.TextSelectionMenuOrdering.COUNT);
    }

    @Override
    public void modifyDefaultMenuItems(
            List<SelectionMenuItem.Builder> menuItemBuilders,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            @NonNull String selectedText) {
        if (!shouldUseSamsungMenuItemOrdering()) return;
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
            ResolveInfo translateResolveInfo = null;
            for (ResolveInfo resolveInfo : textProcessActivities) {
                if (resolveInfo.activityInfo.packageName.equals(TRANSLATOR_PACKAGE_NAME)) {
                    translateResolveInfo = resolveInfo;
                    break;
                }
            }
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
        if (shouldAddWritingToolkitMenu(selectedText, isSelectionPassword)) {
            menuItemBuilders.add(
                    new SelectionMenuItem.Builder(SCAN_TEXT_ID)
                            .setId(SCAN_TEXT_ID)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_ALWAYS)
                            .setOrderInCategory(SamsungDefaultItemOrder.WRITING_TOOLKIT)
                            .setIntent(
                                    createWritingToolkitIntent(selectedText, isSelectionReadOnly)));
        }
    }

    /**
     * Filters text processing activities. It should only be called if isManageAppsSupported returns
     * true.
     *
     * @param activities list of text processing activities to be filtered.
     * @return filtered list of text processing activities.
     */
    @Override
    public List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities) {
        if (!isManageAppsSupported()) {
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

    @NonNull
    @Override
    public List<SelectionMenuItem> getAdditionalTextProcessingItems() {
        if (!isManageAppsSupported()) {
            return new ArrayList<>();
        }
        List<SelectionMenuItem> additionalItemBuilderList = new ArrayList<>();
        additionalItemBuilderList.add(
                new SelectionMenuItem.Builder(
                                org.chromium.android_webview.R.string.actionbar_manage_apps)
                        .setId(Menu.NONE)
                        .setIcon(null)
                        .setOrderInCategory(Menu.CATEGORY_SECONDARY)
                        .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                        .setClickListener(null)
                        .setIntent(createManageAppsIntent())
                        .build());
        return additionalItemBuilderList;
    }

    @Override
    public boolean canReuseCachedSelectionMenu() {
        return !isManageAppsSupported();
    }

    public String getTextFromAdditionalActivityResult(
            int requestCode, int resultCode, Intent data) {
        if (requestCode != WRITING_TOOLKIT_REQUEST_CODE) {
            return null;
        }
        if (resultCode == Activity.RESULT_OK && data != null) {
            CharSequence writingToolkitText =
                    data.getCharSequenceExtra(TEXT_KEY_FROM_WRITING_TOOLKIT);
            CharSequence action = data.getCharSequenceExtra(ACTION_KEY_FROM_WRITING_TOOLKIT);
            if (TextUtils.equals(action, REPLACE_ACTION_KEY_FROM_WRITING_TOOLKIT)
                    && writingToolkitText != null) {
                return writingToolkitText.toString();
            }
        }
        return null;
    }

    public static boolean shouldUseSamsungMenuItemOrdering() {
        return Build.VERSION.SDK_INT <= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && isSamsungDevice()
                && ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION);
    }

    private static int getMenuItemOrder(@IdRes int id) {
        if (id == org.chromium.android_webview.R.id.select_action_menu_cut) {
            return SamsungDefaultItemOrder.CUT;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_copy) {
            return SamsungDefaultItemOrder.COPY;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_paste) {
            return SamsungDefaultItemOrder.PASTE;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_select_all) {
            return SamsungDefaultItemOrder.SELECT_ALL;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_share) {
            return SamsungDefaultItemOrder.SHARE;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_paste_as_plain_text) {
            return SamsungDefaultItemOrder.PASTE_AS_PLAIN_TEXT;
        } else if (id == org.chromium.android_webview.R.id.select_action_menu_web_search) {
            return SamsungDefaultItemOrder.WEB_SEARCH;
        }
        return -1;
    }

    private static boolean isManageAppsSupported() {
        if (sIsManageAppsSupported != null) {
            return sIsManageAppsSupported;
        }
        if (!isSamsungDevice()
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                || Build.VERSION.SDK_INT > Build.VERSION_CODES.VANILLA_ICE_CREAM
                || !ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION)) {
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
                    (selectedText.length() >= MAX_SHARE_QUERY_LENGTH_BYTES)
                            ? selectedText.substring(0, MAX_SHARE_QUERY_LENGTH_BYTES) + "…"
                            : selectedText;
            Intent intent =
                    createProcessTextIntent()
                            .setClassName(info.activityInfo.packageName, info.activityInfo.name)
                            .putExtra(Intent.EXTRA_PROCESS_TEXT, textForProcessing);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        };
    }

    public static boolean handleMenuItemClick(
            MenuItem item, WebContents webContents, ViewGroup containerView) {
        if (!isWritingToolKitMenuItem(item)) return false;
        prepareForWritingToolkit(containerView);
        SelectionPopupController selectionPopupController =
                SelectionPopupController.fromWebContents(webContents);
        selectionPopupController.setPreserveSelectionOnNextLossOfFocus(true);
        Activity parentActivity = ContextUtils.activityFromContext(containerView.getContext());
        // If WebView exists in application context, then activity context can be null for the
        // WebView. To support writing toolkit in such activities, we are just starting writing
        // toolkit activity and discard any result. This is similar to how menu item with
        // PROCESS_TEXT intent are handled.
        if (parentActivity == null) {
            startActivity(item.getIntent());
        } else {
            AwContents awContents = AwContents.fromWebContents(webContents);
            awContents.startActivityForResult(item.getIntent(), WRITING_TOOLKIT_REQUEST_CODE);
        }
        return true;
    }

    private static boolean isWritingToolKitMenuItem(MenuItem item) {
        return SCAN_TEXT_ID != 0 && item.getItemId() == SCAN_TEXT_ID;
    }

    private static void prepareForWritingToolkit(View containerView) {
        Context context = ContextUtils.getApplicationContext();
        InputMethodManager inputMethodManager =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (inputMethodManager != null) {
            inputMethodManager.sendAppPrivateCommand(
                    containerView, WRITING_TOOLKIT_HBD, /* data= */ null);
        }
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
            @NonNull String selectedText, boolean isSelectionPassword) {
        return isSamsungDevice()
                && Build.VERSION.SDK_INT == Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION)
                && !selectedText.isEmpty()
                && !isSelectionPassword
                && SCAN_TEXT_ID != 0
                && isWritingToolkitActivityAvailable();
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
