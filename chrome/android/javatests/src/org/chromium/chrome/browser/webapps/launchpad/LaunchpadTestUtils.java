// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static org.junit.Assert.assertNotNull;

import android.graphics.Bitmap;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Utility class for Launchpad instrumentation tests.
 */
public class LaunchpadTestUtils {
    public static final String APP_PACKAGE_NAME_1 = "package.name.1";
    public static final String APP_NAME_1 = "App Name 1";
    public static final String APP_SHORT_NAME_1 = "App 1";
    public static final String APP_URL_1 = "https://example1.com/";

    public static final String APP_PACKAGE_NAME_2 = "package.name.2";
    public static final String APP_NAME_2 = "App Name 2";
    public static final String APP_SHORT_NAME_2 = "App 2 with long short name";
    public static final String APP_URL_2 = "https://example2.com/";
    public static final Bitmap TEST_ICON = Bitmap.createBitmap(20, 20, Bitmap.Config.ARGB_8888);

    public static final String SHORTCUT_NAME_1 = "Test Shortcut 1";
    public static final String SHORTCUT_URL_1 = "https://example1.com/11";
    public static final String SHORTCUT_NAME_2 = "Test Shortcut 2";
    public static final String SHORTCUT_URL_2 = "https://example1.com/22";

    public static final List<ShortcutItem> MOCK_SHORTCUTS = new ArrayList<>(
            Arrays.asList(new ShortcutItem(SHORTCUT_NAME_1, SHORTCUT_NAME_1, SHORTCUT_URL_1,
                                  "iconUrl", "iconHash", new WebappIcon(TEST_ICON)),
                    new ShortcutItem(SHORTCUT_NAME_2, SHORTCUT_NAME_2, SHORTCUT_URL_2, "iconUrl",
                            "iconHash", new WebappIcon(TEST_ICON))));
    // LaunchpadItem 1 with shortcuts.
    public static final LaunchpadItem LAUNCHPAD_ITEM_1 = new LaunchpadItem(
            APP_PACKAGE_NAME_1, APP_SHORT_NAME_1, APP_NAME_1, APP_URL_1, TEST_ICON, MOCK_SHORTCUTS);
    // LaunchpadItem 2 with no shortcuts.
    public static final LaunchpadItem LAUNCHPAD_ITEM_2 = new LaunchpadItem(APP_PACKAGE_NAME_2,
            APP_SHORT_NAME_2, APP_NAME_2, APP_URL_2, TEST_ICON, new ArrayList<ShortcutItem>());

    // A mock app list, 1st item includes a list of ShortcutItems, 2nd item has no shortcuts.
    public static final List<LaunchpadItem> MOCK_APP_LIST =
            new ArrayList<>(Arrays.asList(LAUNCHPAD_ITEM_1, LAUNCHPAD_ITEM_2));

    private LaunchpadTestUtils() {}

    /**
     * Set permissions to run the test. Notifications set to Block, Mic set to Allow,
     * Camera set to Allow, Location set to ASK.
     */
    public static void setPermissionDefaults(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            PermissionInfo notifications = new PermissionInfo(ContentSettingsType.NOTIFICATIONS,
                    url, null /* embedder */, false /* isEmbargoed */);
            notifications.setContentSetting(profile, ContentSettingValues.BLOCK);
            PermissionInfo mic = new PermissionInfo(ContentSettingsType.MEDIASTREAM_MIC, url,
                    null /* embedder */, false /* isEmbargoed */);
            mic.setContentSetting(profile, ContentSettingValues.ALLOW);
            PermissionInfo camera = new PermissionInfo(ContentSettingsType.MEDIASTREAM_CAMERA, url,
                    null /* embedder */, false /* isEmbargoed */);
            camera.setContentSetting(profile, ContentSettingValues.ALLOW);
            PermissionInfo location = new PermissionInfo(ContentSettingsType.GEOLOCATION, url,
                    null /* embedder */, false /* isEmbargoed */);
            location.setContentSetting(profile, ContentSettingValues.ASK);
        });
    }

    /**
     * Helper function for opening the Management Menu for the LaunchpadItem at {@link itemIndex}
     * and return the dialog view.
     */
    public static View openAppManagementMenu(
            LaunchpadCoordinator coordinator, ModalDialogManager dialogManager, int itemIndex) {
        View item = ((RecyclerView) coordinator.getView().findViewById(R.id.launchpad_recycler))
                            .getChildAt(itemIndex);
        TouchCommon.longPressView(item);
        PropertyModel dialogModel = dialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        return dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
    }
}
