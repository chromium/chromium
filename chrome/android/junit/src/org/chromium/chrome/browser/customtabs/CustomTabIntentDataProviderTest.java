// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.device.mojom.ScreenOrientationLockType;

import java.util.ArrayList;
import java.util.Collections;

/** Tests for {@link CustomTabIntentDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabIntentDataProviderTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String BUTTON_DESCRIPTION = "buttonDescription";

    @Test
    public void colorSchemeParametersAreRetrieved() {
        CustomTabColorSchemeParams lightParams = new CustomTabColorSchemeParams.Builder()
                .setToolbarColor(0xff0000ff)
                .setSecondaryToolbarColor(0xff00aaff)
                .setNavigationBarColor(0xff112233)
                .build();
        CustomTabColorSchemeParams darkParams = new CustomTabColorSchemeParams.Builder()
                .setToolbarColor(0xffff0000)
                .setSecondaryToolbarColor(0xffff8800)
                .setNavigationBarColor(0xff332211)
                .build();
        Intent intent = new CustomTabsIntent.Builder()
                .setColorSchemeParams(COLOR_SCHEME_LIGHT, lightParams)
                .setColorSchemeParams(COLOR_SCHEME_DARK, darkParams)
                .build()
                .intent;

        CustomTabIntentDataProvider lightProvider = new CustomTabIntentDataProvider(
                intent, ApplicationProvider.getApplicationContext(), COLOR_SCHEME_LIGHT);
        CustomTabIntentDataProvider darkProvider = new CustomTabIntentDataProvider(
                intent, ApplicationProvider.getApplicationContext(), COLOR_SCHEME_DARK);

        assertEquals((int) lightParams.toolbarColor, lightProvider.getToolbarColor());
        assertEquals((int) darkParams.toolbarColor, darkProvider.getToolbarColor());

        assertEquals((int) lightParams.secondaryToolbarColor, lightProvider.getBottomBarColor());
        assertEquals((int) darkParams.secondaryToolbarColor, darkProvider.getBottomBarColor());

        assertEquals(lightParams.navigationBarColor, lightProvider.getNavigationBarColor());
        assertEquals(darkParams.navigationBarColor, darkProvider.getNavigationBarColor());
    }

    /* Test the setting the default orientation for Trusted Web Activity and getting the default
     * orientation.
     */
    @Test
    public void defaultOrientationIsSet() {
        Context mContext = ApplicationProvider.getApplicationContext();
        CustomTabsSession mSession = CustomTabsSession.createMockSessionForTesting(
                new ComponentName(mContext, ChromeLauncherActivity.class));

        TrustedWebActivityIntentBuilder twaBuilder =
                new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                        .setScreenOrientation(ScreenOrientation.LANDSCAPE);
        Intent intent = twaBuilder.build(mSession).getIntent();
        CustomTabIntentDataProvider customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(ScreenOrientationLockType.LANDSCAPE,
                customTabIntentDataProvider.getDefaultOrientation());

        twaBuilder = new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                             .setScreenOrientation(ScreenOrientation.PORTRAIT);
        intent = twaBuilder.build(mSession).getIntent();
        customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(ScreenOrientationLockType.PORTRAIT,
                customTabIntentDataProvider.getDefaultOrientation());
    }

    @Test
    public void shareStateDefault_noButtonInToolbar_hasShareInToolbar() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = new Intent().putExtra(
                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_DEFAULT);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertEquals(context.getResources().getString(R.string.share),
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
    }

    @Test
    public void shareStateDefault_buttonInToolbar_hasShareItemInMenu() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = new Intent()
                                .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                        CustomTabsIntent.SHARE_STATE_DEFAULT)
                                .putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                        createActionButtonInToolbarBundle());

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateDefault_buttonInToolbarAndCustomMenuItems_hasNoShare() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_DEFAULT)
                        .putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                createActionButtonInToolbarBundle())
                        .putExtra(CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOn_buttonInToolbar_hasShareItemInMenu() {
        Context context = ApplicationProvider.getApplicationContext();
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent = new Intent()
                                .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                        CustomTabsIntent.SHARE_STATE_ON)
                                .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOn_buttonInToolbarAndCustomMenuItems_hasNoShare() {
        Context context = ApplicationProvider.getApplicationContext();
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOff_noShareItems() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = new Intent().putExtra(
                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_OFF);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, context, COLOR_SCHEME_LIGHT);

        assertTrue(dataProvider.getCustomButtonsOnToolbar().isEmpty());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    private Bundle createActionButtonInToolbarBundle() {
        Context context = ApplicationProvider.getApplicationContext();
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);
        int iconHeight = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON,
                Bitmap.createBitmap(iconHeight, iconHeight, Bitmap.Config.ALPHA_8));
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, BUTTON_DESCRIPTION);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(context, 0, new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    private Bundle createMenuItemBundle() {
        Context context = ApplicationProvider.getApplicationContext();
        Bundle bundle = new Bundle();
        bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, "title");
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(context, 0, new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        return bundle;
    }

    protected Uri getLaunchingUrl() {
        return Uri.parse("https://www.example.com/");
    }
}
