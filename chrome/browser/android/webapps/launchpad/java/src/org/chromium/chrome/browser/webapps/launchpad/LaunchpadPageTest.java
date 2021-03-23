// Copyright 2021 The Chromium Authors. All rights reserved.
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
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for the Launchpad page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.APP_LAUNCHPAD})
public class LaunchpadPageTest {
    private static final String APP_PACKAGE_NAME_1 = "package.name.1";
    private static final String APP_NAME_1 = "App Name 1";
    private static final String APP_SHORT_NAME_1 = "App 1";
    private static final String APP_URL_1 = "https://example.com/1";
    private static final String APP_PACKAGE_NAME_2 = "package.name.2";
    private static final String APP_NAME_2 = "App Name 2";
    private static final String APP_SHORT_NAME_2 = "App 2 with long short name";
    private static final String APP_URL_2 = "https://example.com/2";
    private static final Bitmap TEST_ICON = Bitmap.createBitmap(20, 20, Bitmap.Config.ARGB_8888);

    private static final List<LaunchpadItem> MOCK_APP_LIST =
            new ArrayList<>(Arrays.asList(new LaunchpadItem(APP_PACKAGE_NAME_1, APP_SHORT_NAME_1,
                                                  APP_NAME_1, APP_URL_1, TEST_ICON),
                    new LaunchpadItem(APP_PACKAGE_NAME_2, APP_SHORT_NAME_2, APP_NAME_2, APP_URL_2,
                            TEST_ICON)));

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private LaunchpadCoordinator mLaunchpadCoordinator;

    private RecyclerView mItemContainer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void openLaunchpadPage() {
        mLaunchpadCoordinator = new LaunchpadCoordinator(mActivityTestRule.getActivity(),
                mActivityTestRule.getActivity().getModalDialogManagerSupplier(), MOCK_APP_LIST);

        LaunchpadPage.setCoordinatorForTesting(mLaunchpadCoordinator);

        mActivityTestRule.loadUrl(UrlConstants.LAUNCHPAD_URL);
        mItemContainer = mActivityTestRule.getActivity().findViewById(R.id.launchpad_recycler);
    }

    @Test
    @SmallTest
    public void testOpenLaunchpad() {
        openLaunchpadPage();
        Assert.assertEquals(2, mItemContainer.getAdapter().getItemCount());

        TileView app1 = (TileView) mItemContainer.getChildAt(0);
        TextView title1 = (TextView) app1.findViewById(R.id.tile_view_title);
        ImageView icon1 = (ImageView) app1.findViewById(R.id.tile_view_icon);
        Assert.assertEquals(APP_SHORT_NAME_1, title1.getText());
        Assert.assertEquals(1, title1.getLineCount());
        Assert.assertEquals(TEST_ICON, ((BitmapDrawable) icon1.getDrawable()).getBitmap());

        TileView app2 = (TileView) mItemContainer.getChildAt(1);
        TextView title2 = (TextView) app2.findViewById(R.id.tile_view_title);
        ImageView icon2 = (ImageView) app2.findViewById(R.id.tile_view_icon);
        Assert.assertEquals(APP_SHORT_NAME_2, title2.getText());
        Assert.assertEquals(1, title2.getLineCount());
        Assert.assertEquals(TEST_ICON, ((BitmapDrawable) icon2.getDrawable()).getBitmap());
    }

    @Test
    @SmallTest
    public void testLaunchWebApk() {
        openLaunchpadPage();

        Intents.init();
        intending(allOf(hasPackage(APP_PACKAGE_NAME_1)))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        View item = mItemContainer.getChildAt(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(item));

        intended(allOf(hasPackage(APP_PACKAGE_NAME_1), hasData(APP_URL_1)), times(1));
        Intents.release();
    }

    @Test
    @SmallTest
    public void testShowAppManagementMenu() {
        openLaunchpadPage();
        ModalDialogManager modalDialogManager =
                mActivityTestRule.getActivity().getModalDialogManager();

        View item = mItemContainer.getChildAt(1);
        TouchCommon.longPressView(item);

        PropertyModel dialogModel = modalDialogManager.getCurrentDialogForTest();
        Assert.assertNotNull(dialogModel);

        View dialogView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Assert.assertEquals(
                APP_NAME_2, ((TextView) dialogView.findViewById(R.id.menu_header_title)).getText());
        Assert.assertEquals(
                APP_URL_2, ((TextView) dialogView.findViewById(R.id.menu_header_url)).getText());
        ImageView icon = (ImageView) dialogView.findViewById(R.id.menu_header_image);
        Assert.assertEquals(TEST_ICON, ((BitmapDrawable) icon.getDrawable()).getBitmap());
    }
}
