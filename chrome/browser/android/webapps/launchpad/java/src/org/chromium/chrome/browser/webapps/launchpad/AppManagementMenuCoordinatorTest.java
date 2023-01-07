// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link AppManagementMenuCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppManagementMenuCoordinatorTest {
    public static final String APP_PACKAGE_NAME = "package.name";
    public static final String APP_SHORT_NAME = "App";
    public static final String APP_NAME = "App Name";
    public static final String APP_URL = "https://example.com/";
    public final Bitmap APP_ICON = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    public final LaunchpadItem MOCK_ITEM = new LaunchpadItem(APP_PACKAGE_NAME, APP_SHORT_NAME,
            APP_NAME, APP_URL, APP_ICON, new ArrayList<ShortcutItem>());

    public static final String SHORTCUT_NAME_1 = "Shortcut 1";
    public static final String SHORTCUT_URL_1 = "https://example.com/11";
    public static final String SHORTCUT_NAME_2 = "Shortcut 2";
    public static final String SHORTCUT_URL_2 = "https://example.com/22";
    public final Bitmap SHORTCUT_ICON = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888);
    public final ShortcutItem SHORTCUT_ITEM_1 = new ShortcutItem(SHORTCUT_NAME_1, SHORTCUT_NAME_1,
            SHORTCUT_URL_1, "iconUrl", "iconHash", new WebappIcon(SHORTCUT_ICON));
    public final ShortcutItem SHORTCUT_ITEM_2 = new ShortcutItem(SHORTCUT_NAME_2, SHORTCUT_NAME_2,
            SHORTCUT_URL_2, "iconUrl", "iconHash", new WebappIcon(SHORTCUT_ICON));

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private PackageManager mPackageManager;

    @Mock
    private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    @Mock
    private SettingsLauncher mSettingsLauncher;

    private Activity mActivity;
    private AppManagementMenuCoordinator mCoordinator;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        Profile.setLastUsedProfileForTesting(Mockito.mock(Profile.class));

        mModalDialogManager =
                new ModalDialogManager(Mockito.mock(ModalDialogManager.Presenter.class), 0);
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        modalDialogManagerSupplier.set(mModalDialogManager);

        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
        when(mActivity.getPackageManager()).thenReturn(mPackageManager);

        mCoordinator = new AppManagementMenuCoordinator(
                mActivity, modalDialogManagerSupplier, mSettingsLauncher);
    }

    @Test
    public void testShowDialog() {
        mCoordinator.show(MOCK_ITEM);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);

        View dialogView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        // Assert header view.
        assertEquals(
                APP_NAME, ((TextView) dialogView.findViewById(R.id.menu_header_title)).getText());
        assertEquals(APP_URL, ((TextView) dialogView.findViewById(R.id.menu_header_url)).getText());
        ImageView icon = (ImageView) dialogView.findViewById(R.id.menu_header_image);
        assertEquals(APP_ICON, ((BitmapDrawable) icon.getDrawable()).getBitmap());

        // Assert permissions view.
        assertNotNull(dialogView.findViewById(R.id.notifications_button));
        assertNotNull(dialogView.findViewById(R.id.mic_button));
        assertNotNull(dialogView.findViewById(R.id.camera_button));
        assertNotNull(dialogView.findViewById(R.id.location_button));
    }

    @Test
    public void testShowDialogWithShortcuts() {
        List<ShortcutItem> shortcuts = new ArrayList<>();
        shortcuts.add(SHORTCUT_ITEM_1);
        shortcuts.add(SHORTCUT_ITEM_2);
        LaunchpadItem item = new LaunchpadItem(
                APP_PACKAGE_NAME, APP_SHORT_NAME, APP_NAME, APP_URL, APP_ICON, shortcuts);

        ListView shortcutsView = openDialogAndGetShortcutsListView(item);
        assertNotNull(shortcutsView);

        assertEquals(4, shortcutsView.getAdapter().getCount());

        // Assert the shortcuts list model are set correctly.
        ListItem listItem = (ListItem) shortcutsView.getAdapter().getItem(0);
        assertEquals(SHORTCUT_NAME_1, listItem.model.get(ShortcutItemProperties.NAME));
        assertEquals(SHORTCUT_URL_1, listItem.model.get(ShortcutItemProperties.LAUNCH_URL));
        assertEquals(SHORTCUT_ICON, listItem.model.get(ShortcutItemProperties.SHORTCUT_ICON));
        assertNotNull(listItem.model.get(ShortcutItemProperties.ON_CLICK));

        ListItem listItem2 = (ListItem) shortcutsView.getAdapter().getItem(1);
        assertEquals(SHORTCUT_NAME_2, listItem2.model.get(ShortcutItemProperties.NAME));
        assertEquals(SHORTCUT_URL_2, listItem2.model.get(ShortcutItemProperties.LAUNCH_URL));
        assertEquals(SHORTCUT_ICON, listItem2.model.get(ShortcutItemProperties.SHORTCUT_ICON));
        assertNotNull(listItem2.model.get(ShortcutItemProperties.ON_CLICK));
    }

    @Test
    public void testUninstallMenuItem() {
        ListView shortcutsView = openDialogAndGetShortcutsListView(MOCK_ITEM);

        assertEquals(2, shortcutsView.getAdapter().getCount());
        ListItem uninstallItem = (ListItem) shortcutsView.getAdapter().getItem(0);
        assertEquals(AppManagementMenuCoordinator.ListItemType.MENU_ITEM, uninstallItem.type);
        assertEquals(mActivity.getResources().getString(R.string.launchpad_menu_uninstall),
                uninstallItem.model.get(ShortcutItemProperties.NAME));
        assertNull(uninstallItem.model.get(ShortcutItemProperties.LAUNCH_URL));
        assertNull(uninstallItem.model.get(ShortcutItemProperties.SHORTCUT_ICON));
        assertTrue(uninstallItem.model.get(ShortcutItemProperties.HIDE_ICON));
        assertNotNull(uninstallItem.model.get(ShortcutItemProperties.ON_CLICK));
    }

    @Test
    public void testSiteSettingMenuItem() {
        ListView shortcutsView = openDialogAndGetShortcutsListView(MOCK_ITEM);

        assertEquals(2, shortcutsView.getAdapter().getCount());
        ListItem settingItem = (ListItem) shortcutsView.getAdapter().getItem(1);
        assertEquals(AppManagementMenuCoordinator.ListItemType.MENU_ITEM, settingItem.type);
        assertEquals(mActivity.getResources().getString(R.string.launchpad_menu_site_settings),
                settingItem.model.get(ShortcutItemProperties.NAME));
        assertNull(settingItem.model.get(ShortcutItemProperties.LAUNCH_URL));
        assertNull(settingItem.model.get(ShortcutItemProperties.SHORTCUT_ICON));
        assertTrue(settingItem.model.get(ShortcutItemProperties.HIDE_ICON));
        assertNotNull(settingItem.model.get(ShortcutItemProperties.ON_CLICK));
    }

    @Test
    public void testClickUninstallMenuItem() throws NameNotFoundException {
        ListView shortcutsView = openDialogAndGetShortcutsListView(MOCK_ITEM);
        ListItem uninstallItem = (ListItem) shortcutsView.getAdapter().getItem(0);

        PackageInfo packageInfo = Mockito.mock(PackageInfo.class);
        when(mPackageManager.getPackageInfo(eq(APP_PACKAGE_NAME), anyInt()))
                .thenReturn(packageInfo);

        uninstallItem.model.get(ShortcutItemProperties.ON_CLICK)
                .onClick(shortcutsView.getChildAt(0));

        verify(mActivity).startActivity(notNull());
    }

    @Test
    public void testClickUninstallMenuItem_appNotExist() throws NameNotFoundException {
        ListView shortcutsView = openDialogAndGetShortcutsListView(MOCK_ITEM);
        ListItem uninstallItem = (ListItem) shortcutsView.getAdapter().getItem(0);

        when(mPackageManager.getPackageInfo(eq(APP_PACKAGE_NAME), anyInt()))
                .thenThrow(new PackageManager.NameNotFoundException());

        uninstallItem.model.get(ShortcutItemProperties.ON_CLICK)
                .onClick(shortcutsView.getChildAt(0));

        verify(mActivity, never()).startActivity(notNull());
    }

    public ListView openDialogAndGetShortcutsListView(LaunchpadItem item) {
        mCoordinator.show(item);
        View dialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        return dialogView.findViewById(R.id.shortcuts_list_view);
    }
}
