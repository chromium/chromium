// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/**
 * Coordinator for displaying the app management menu.
 */
class AppManagementMenuCoordinator implements ModalDialogProperties.Controller {
    private static final String TAG = "LaunchpadManageMenu";

    private final Context mContext;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final SettingsLauncher mSettingsLauncher;

    private PropertyModel mDialogModel;

    private ListView mShortcutList;

    public @interface ListItemType {
        // The type for each shortcut of the WebAPK.
        int SHORTCUT_ITEM = 0;
        // The type for each menu item all apps have (Uninstall, Settings, etc).
        int MENU_ITEM = 1;
    }

    private AppManagementMenuPermissionsCoordinator mPermissionsCoordinator;

    AppManagementMenuCoordinator(Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            SettingsLauncher settingsLauncher) {
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSettingsLauncher = settingsLauncher;
    }

    void destroy() {
        mModalDialogManagerSupplier.get().dismissDialog(
                mDialogModel, DialogDismissalCause.TAB_DESTROYED);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDialogModel = null;
        if (mPermissionsCoordinator != null) {
            mPermissionsCoordinator.destroy();
            mPermissionsCoordinator = null;
        }
    }

    void show(LaunchpadItem item) {
        mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                               .with(ModalDialogProperties.CONTROLLER, this)
                               .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                               .with(ModalDialogProperties.CUSTOM_VIEW, createDialogView(item))
                               .build();

        mModalDialogManagerSupplier.get().showDialog(
                mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private View createDialogView(LaunchpadItem item) {
        View dialogView = LayoutInflater.from(mContext).inflate(
                R.layout.launchpad_menu_dialog_layout, null, false);

        View headerView = dialogView.findViewById(R.id.dialog_header);
        PropertyModel headerModel = AppManagementMenuHeaderProperties.buildHeader(item);
        PropertyModelChangeProcessor.create(
                headerModel, headerView, new AppManagementMenuHeaderViewBinder());

        mPermissionsCoordinator = new AppManagementMenuPermissionsCoordinator(mContext,
                (AppManagementMenuPermissionsView) dialogView.findViewById(R.id.permissions), item);

        mShortcutList = dialogView.findViewById(R.id.shortcuts_list_view);

        ModelList listItems = buildShortcutsList(item);
        listItems.add(buildMenuItem(R.string.launchpad_menu_uninstall,
                (v) -> onUninstallMenuItemClicked(item.packageName)));
        listItems.add(buildMenuItem(R.string.launchpad_menu_site_settings,
                (v) -> onSiteSettingMenuItemClick(item.url)));

        ModelListAdapter adapter = new ModelListAdapter(listItems);
        mShortcutList.setAdapter(adapter);
        adapter.registerType(ListItemType.SHORTCUT_ITEM,
                new LayoutViewBuilder(R.layout.launchpad_shortcut_item_view),
                ShortcutItemViewBinder::bind);
        adapter.registerType(ListItemType.MENU_ITEM,
                new LayoutViewBuilder(R.layout.launchpad_shortcut_item_view),
                ShortcutItemViewBinder::bind);
        return dialogView;
    }

    private ModelList buildShortcutsList(LaunchpadItem item) {
        ModelList list = new ModelList();
        if (item.shortcutItems == null) return list;

        for (ShortcutItem shortcutItem : item.shortcutItems) {
            PropertyModel model =
                    new PropertyModel.Builder(ShortcutItemProperties.ALL_KEYS)
                            .with(ShortcutItemProperties.NAME, shortcutItem.name)
                            .with(ShortcutItemProperties.LAUNCH_URL, shortcutItem.launchUrl)
                            .with(ShortcutItemProperties.SHORTCUT_ICON, shortcutItem.icon.bitmap())
                            .with(ShortcutItemProperties.ON_CLICK,
                                    v
                                    -> onShortcutClicked(item.packageName, shortcutItem.launchUrl))
                            .build();

            list.add(new ListItem(ListItemType.SHORTCUT_ITEM, model));
        }
        return list;
    }

    private ListItem buildMenuItem(int name, View.OnClickListener onClick) {
        PropertyModel model = new PropertyModel.Builder(ShortcutItemProperties.ALL_KEYS)
                                      .with(ShortcutItemProperties.NAME, mContext.getString(name))
                                      .with(ShortcutItemProperties.HIDE_ICON, true)
                                      .with(ShortcutItemProperties.ON_CLICK, onClick)
                                      .build();
        return new ListItem(ListItemType.MENU_ITEM, model);
    }

    private void onShortcutClicked(String packageName, String launchUrl) {
        Intent launchIntent =
                WebApkNavigationClient.createLaunchWebApkIntent(packageName, launchUrl, false);
        try {
            mContext.startActivity(launchIntent);
        } catch (ActivityNotFoundException e) {
            Toast.makeText(mContext, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
        }

        mModalDialogManagerSupplier.get().dismissDialog(
                mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private void onUninstallMenuItemClicked(String packageName) {
        if (!PackageUtils.isPackageInstalled(mContext, packageName)) {
            Log.e(TAG, "WebApk not found:" + packageName);
            return;
        }

        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setData(Uri.parse("package:" + packageName));
        mContext.startActivity(intent);
        mModalDialogManagerSupplier.get().dismissDialog(
                mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private void onSiteSettingMenuItemClick(String url) {
        Bundle args = SingleWebsiteSettings.createFragmentArgsForSite(url);
        mSettingsLauncher.launchSettingsActivity(mContext, SingleWebsiteSettings.class, args);
    }
}
