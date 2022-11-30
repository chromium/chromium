// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.Intents.times;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasPackage;

import static org.hamcrest.Matchers.allOf;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Tests for the Launchpad page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.APP_LAUNCHPAD})
public class LaunchpadPageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private LaunchpadCoordinator mLaunchpadCoordinator;

    private RecyclerView mItemContainer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        LaunchpadUtils.setOverrideItemListForTesting(LaunchpadTestUtils.MOCK_APP_LIST);
    }

    private void openLaunchpadPage() {
        mActivityTestRule.loadUrl(UrlConstants.LAUNCHPAD_URL);
        mLaunchpadCoordinator =
                ((LaunchpadPage) mActivityTestRule.getActivity().getActivityTab().getNativePage())
                        .getCoordinatorForTesting();
        mItemContainer = mLaunchpadCoordinator.getView().findViewById(R.id.launchpad_recycler);
    }

    @Test
    @SmallTest
    public void testOpenLaunchpad() {
        openLaunchpadPage();

        Assert.assertEquals(2, mItemContainer.getAdapter().getItemCount());

        TileView app1 = (TileView) mItemContainer.getChildAt(0);
        TextView title1 = (TextView) app1.findViewById(R.id.tile_view_title);
        ImageView icon1 = (ImageView) app1.findViewById(R.id.tile_view_icon);
        Assert.assertEquals(LaunchpadTestUtils.APP_SHORT_NAME_1, title1.getText());
        Assert.assertEquals(1, title1.getLineCount());
        Assert.assertEquals(
                LaunchpadTestUtils.TEST_ICON, ((BitmapDrawable) icon1.getDrawable()).getBitmap());

        TileView app2 = (TileView) mItemContainer.getChildAt(1);
        TextView title2 = (TextView) app2.findViewById(R.id.tile_view_title);
        ImageView icon2 = (ImageView) app2.findViewById(R.id.tile_view_icon);
        Assert.assertEquals(LaunchpadTestUtils.APP_SHORT_NAME_2, title2.getText());
        Assert.assertEquals(1, title2.getLineCount());
        Assert.assertEquals(
                LaunchpadTestUtils.TEST_ICON, ((BitmapDrawable) icon2.getDrawable()).getBitmap());
    }

    @Test
    @SmallTest
    public void testLaunchWebApk() {
        openLaunchpadPage();

        Intents.init();
        intending(allOf(hasPackage(LaunchpadTestUtils.APP_PACKAGE_NAME_1)))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        View item = mItemContainer.getChildAt(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(item));

        intended(allOf(hasPackage(LaunchpadTestUtils.APP_PACKAGE_NAME_1),
                         hasData(LaunchpadTestUtils.APP_URL_1)),
                times(1));
        Intents.release();
    }

    @Test
    @MediumTest
    public void testManagementMenuHeaderProperties() {
        openLaunchpadPage();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mActivityTestRule.getActivity().getModalDialogManager(), 1 /* itemIndex */);

        Assert.assertEquals(LaunchpadTestUtils.APP_NAME_2,
                ((TextView) dialogView.findViewById(R.id.menu_header_title)).getText());
        Assert.assertEquals(LaunchpadTestUtils.APP_URL_2,
                ((TextView) dialogView.findViewById(R.id.menu_header_url)).getText());
        ImageView icon = (ImageView) dialogView.findViewById(R.id.menu_header_image);
        Assert.assertEquals(
                LaunchpadTestUtils.TEST_ICON, ((BitmapDrawable) icon.getDrawable()).getBitmap());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1271233")
    public void testManagementMenuAppPermissions() {
        LaunchpadTestUtils.setPermissionDefaults(LaunchpadTestUtils.APP_URL_2);
        openLaunchpadPage();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mActivityTestRule.getActivity().getModalDialogManager(), 1 /* itemIndex */);

        // Icons for permissions that set to "ALLOW" or "BLOCK" are enabled.
        int[] enabledIcons = {R.id.notifications_button, R.id.mic_button, R.id.camera_button};
        for (int id : enabledIcons) {
            ImageView icon = (ImageView) dialogView.findViewById(id);
            Assert.assertEquals(
                    AppCompatResources.getColorStateList(
                            mActivityTestRule.getActivity(), R.color.default_icon_color_tint_list),
                    icon.getImageTintList());
            Assert.assertTrue(icon.isEnabled());
        }

        ImageView locationIcon = (ImageView) dialogView.findViewById(R.id.location_button);
        Assert.assertEquals(AppCompatResources.getColorStateList(mActivityTestRule.getActivity(),
                                    R.color.default_icon_color_disabled),
                locationIcon.getImageTintList());
        Assert.assertFalse(locationIcon.isEnabled());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1306215")
    public void testManagementMenuAppShortcutsProperties() {
        openLaunchpadPage();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mActivityTestRule.getActivity().getModalDialogManager(), 0 /* itemIndex */);

        ListView listView = dialogView.findViewById(R.id.shortcuts_list_view);

        // The test app has 2 shortcut and 2 other menu item: uninstall and site settings.
        Assert.assertEquals(4, listView.getChildCount());

        // Assert icon and name in shortcut item view is set correctly.
        View shortcut1 = listView.getChildAt(0);
        Assert.assertEquals(LaunchpadTestUtils.SHORTCUT_NAME_1,
                ((TextView) shortcut1.findViewById(R.id.shortcut_name)).getText());
        ImageView icon = (ImageView) shortcut1.findViewById(R.id.shortcut_icon);
        Assert.assertEquals(
                LaunchpadTestUtils.TEST_ICON, ((BitmapDrawable) icon.getDrawable()).getBitmap());
        Assert.assertEquals(View.VISIBLE, icon.getVisibility());

        View shortcut2 = listView.getChildAt(1);
        Assert.assertEquals(LaunchpadTestUtils.SHORTCUT_NAME_2,
                ((TextView) shortcut2.findViewById(R.id.shortcut_name)).getText());
        icon = (ImageView) shortcut2.findViewById(R.id.shortcut_icon);
        Assert.assertEquals(
                LaunchpadTestUtils.TEST_ICON, ((BitmapDrawable) icon.getDrawable()).getBitmap());
        Assert.assertEquals(View.VISIBLE, icon.getVisibility());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1306215")
    public void testLaunchAppShortcuts() {
        openLaunchpadPage();
        ModalDialogManager modalDialogManager =
                mActivityTestRule.getActivity().getModalDialogManager();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(
                mLaunchpadCoordinator, modalDialogManager, 0 /* itemIndex */);

        ListView listView = dialogView.findViewById(R.id.shortcuts_list_view);
        View shortcut = listView.getChildAt(0);

        Intents.init();
        intending(allOf(hasPackage(LaunchpadTestUtils.APP_PACKAGE_NAME_1)))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(shortcut));
        intended(allOf(hasPackage(LaunchpadTestUtils.APP_PACKAGE_NAME_1),
                         hasData(LaunchpadTestUtils.SHORTCUT_URL_1)),
                times(1));
        Intents.release();

        // Assert dialog is dismissed.
        Assert.assertNull(modalDialogManager.getCurrentDialogForTest());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1306215")
    public void testManagementMenuOtherMenuItemProperties() {
        openLaunchpadPage();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mActivityTestRule.getActivity().getModalDialogManager(), 1 /* itemIndex */);

        ListView listView = dialogView.findViewById(R.id.shortcuts_list_view);

        // The test app has no shortcut but 2 other menu items: uninstall and site settings.
        Assert.assertEquals(2, listView.getChildCount());

        // Assert the uninstall item has correct name, and icon visibility is GONE.
        View item1 = listView.getChildAt(0);
        Assert.assertEquals(mActivityTestRule.getActivity().getResources().getString(
                                    R.string.launchpad_menu_uninstall),
                ((TextView) item1.findViewById(R.id.shortcut_name)).getText());
        Assert.assertEquals(View.GONE, item1.findViewById(R.id.shortcut_icon).getVisibility());

        // Assert the site settings item has correct name, and icon visibility is GONE.
        View item2 = listView.getChildAt(1);
        Assert.assertEquals(mActivityTestRule.getActivity().getResources().getString(
                                    R.string.launchpad_menu_site_settings),
                ((TextView) item2.findViewById(R.id.shortcut_name)).getText());
        Assert.assertEquals(View.GONE, item2.findViewById(R.id.shortcut_icon).getVisibility());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1306215")
    public void testLaunchAppSetting_whenSiteSettingsMenuItemClicked() {
        openLaunchpadPage();
        ModalDialogManager modalDialogManager =
                mActivityTestRule.getActivity().getModalDialogManager();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(
                mLaunchpadCoordinator, modalDialogManager, 1 /* itemIndex */);
        ListView listView = dialogView.findViewById(R.id.shortcuts_list_view);

        // Click the menu item and assert site setting page is opened.
        SettingsActivity activity =
                ActivityTestUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class, new Runnable() {
                            @Override
                            public void run() {
                                TouchCommon.singleClickView(listView.getChildAt(1));
                            }
                        });
        Assert.assertNotNull(activity);
        SingleWebsiteSettings fragment =
                ActivityTestUtils.waitForFragmentToAttach(activity, SingleWebsiteSettings.class);
        Assert.assertNotNull(fragment);
        activity.finish();
    }
}
