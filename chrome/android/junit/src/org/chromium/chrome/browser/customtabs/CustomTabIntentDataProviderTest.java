// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_HEIGHT_ADJUSTABLE;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_HEIGHT_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_HEIGHT_FIXED;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_DEFAULT;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_START;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_CORNER_RADIUS_DP;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Network;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomContentAction;
import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.BackgroundInteractBehavior;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams.VariantLayoutType;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.device.mojom.ScreenOrientationLockType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link CustomTabIntentDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@DisableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
public class CustomTabIntentDataProviderTest {

    private static final String BUTTON_DESCRIPTION = "buttonDescription";
    private static final String PACKAGE = "com.example.package.app";

    private Context mContext;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void colorSchemeParametersAreRetrieved() {
        CustomTabColorSchemeParams lightParams =
                new CustomTabColorSchemeParams.Builder()
                        .setToolbarColor(0xff0000ff)
                        .setSecondaryToolbarColor(0xff00aaff)
                        .setNavigationBarColor(0xff112233)
                        .build();
        CustomTabColorSchemeParams darkParams =
                new CustomTabColorSchemeParams.Builder()
                        .setToolbarColor(0xffff0000)
                        .setSecondaryToolbarColor(0xffff8800)
                        .setNavigationBarColor(0xff332211)
                        .build();
        Intent intent =
                new CustomTabsIntent.Builder()
                        .setColorSchemeParams(COLOR_SCHEME_LIGHT, lightParams)
                        .setColorSchemeParams(COLOR_SCHEME_DARK, darkParams)
                        .build()
                        .intent;

        ColorProvider lightProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT)
                        .getColorProvider();
        ColorProvider darkProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_DARK)
                        .getColorProvider();

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
        CustomTabsSession mSession =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));

        TrustedWebActivityIntentBuilder twaBuilder =
                new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                        .setScreenOrientation(ScreenOrientation.LANDSCAPE);
        Intent intent = twaBuilder.build(mSession).getIntent();
        CustomTabIntentDataProvider customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                ScreenOrientationLockType.LANDSCAPE,
                customTabIntentDataProvider.getDefaultOrientation());

        twaBuilder =
                new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                        .setScreenOrientation(ScreenOrientation.PORTRAIT);
        intent = twaBuilder.build(mSession).getIntent();
        customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                ScreenOrientationLockType.PORTRAIT,
                customTabIntentDataProvider.getDefaultOrientation());
    }

    @Test
    public void shareStateDefault_noButtonInToolbar_hasShareInToolbar() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_DEFAULT);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                mContext.getString(R.string.share),
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
    }

    @Test
    public void shareStateDefault_buttonInToolbar_hasShareItemInMenu() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_DEFAULT)
                        .putExtra(
                                CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                createActionButtonInToolbarBundle());

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateDefault_buttonInToolbarAndCustomMenuItems_hasNoShare() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_DEFAULT)
                        .putExtra(
                                CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                createActionButtonInToolbarBundle())
                        .putExtra(
                                CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOn_buttonInToolbar_hasShareItemInMenu() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void googleBottomBarFlagsOff_customButtonWithSupportedId_hasItemsInToolbar() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100),
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(0, dataProvider.getCustomButtonsOnGoogleBottomBar().size());
        assertEquals(2, dataProvider.getCustomButtonsOnToolbar().size());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR})
    public void googleBottomBarFlagsOn_customButtonWithSupportedId_hasItemInGoogleBottomBar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100),
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(1, dataProvider.getCustomButtonsOnGoogleBottomBar().size());
        assertEquals(1, dataProvider.getCustomButtonsOnToolbar().size());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR})
    public void googleBottomBarFlagsOn_customButtonWithNonSupportedId_hasItemsInToolbar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(1),
                                createCustomActionButtonBundleWithId(2)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(0, dataProvider.getCustomButtonsOnGoogleBottomBar().size());
        assertEquals(2, dataProvider.getCustomButtonsOnToolbar().size());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR})
    public void
            googleBottomBarFlagsOn_hasExtraGoogleBottomBarButtons_hasSupportedItemsInGoogleBottomBar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(103),
                                createCustomGoogleBottomBarItemBundleWithId(2)));

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100),
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(2, dataProvider.getCustomButtonsOnGoogleBottomBar().size());
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(1, dataProvider.getCustomButtonsOnToolbar().size());
        assertEquals(3, dataProvider.getAllCustomButtons().size());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR,
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS
    })
    public void googleBottomBarFlagsOn_withNoVariantLayout_hasItemsInGoogleBottomBar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(105), // CUSTOM
                                createCustomGoogleBottomBarItemBundleWithId(2)) // UNSUPPORTED
                        );

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        when(connection.getGoogleBottomBarIntentParams(any()))
                .thenReturn(
                        GoogleBottomBarIntentParams.newBuilder()
                                .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                                .addAllEncodedButton(
                                        // PIH_BASIC, CUSTOM, SEARCH  Not checked for this layout
                                        // type
                                        List.of(0, 1, 8, 9))
                                .build());
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100), // SAVE
                                createCustomActionButtonBundleWithId(101), // SHARE
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                3, dataProvider.getCustomButtonsOnGoogleBottomBar().size()); // SAVE, SHARE, CUSTOM
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(1, dataProvider.getCustomButtonsOnToolbar().size()); // 1
        assertEquals(4, dataProvider.getAllCustomButtons().size());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR,
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS
    })
    public void googleBottomBarFlagsOn_withDoubleDeckerLayout_hasItemsInGoogleBottomBar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(105), // CUSTOM
                                createCustomGoogleBottomBarItemBundleWithId(2)) // UNSUPPORTED
                        );

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        when(connection.getGoogleBottomBarIntentParams(any()))
                .thenReturn(
                        GoogleBottomBarIntentParams.newBuilder()
                                .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                                .addAllEncodedButton(
                                        // PIH_BASIC, CUSTOM, SEARCH  Not checked for this layout
                                        // type
                                        List.of(0, 1, 8, 9))
                                .build());
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100), // SAVE
                                createCustomActionButtonBundleWithId(101), // SHARE
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                3, dataProvider.getCustomButtonsOnGoogleBottomBar().size()); // SAVE, SHARE, CUSTOM
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(1, dataProvider.getCustomButtonsOnToolbar().size()); // 1
        assertEquals(4, dataProvider.getAllCustomButtons().size());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR,
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS
    })
    public void googleBottomBarFlagsOn_withSingleDeckerLayout_hasItemsInToolbar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(105), // CUSTOM
                                createCustomGoogleBottomBarItemBundleWithId(2)) // UNSUPPORTED
                        );

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        when(connection.getGoogleBottomBarIntentParams(any()))
                .thenReturn(
                        GoogleBottomBarIntentParams.newBuilder()
                                .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                                .addAllEncodedButton(List.of())
                                .build());
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100), // SAVE
                                createCustomActionButtonBundleWithId(101))); // SHARE

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(1, dataProvider.getCustomButtonsOnGoogleBottomBar().size()); // CUSTOM
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(2, dataProvider.getCustomButtonsOnToolbar().size()); // SAVE, SHARE
        assertEquals(3, dataProvider.getAllCustomButtons().size());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR,
        ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS
    })
    public void googleBottomBarFlagsOn_withSingleDeckerWithRightButtonsLayout_hasItemsInToolbar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(105), // CUSTOM
                                createCustomGoogleBottomBarItemBundleWithId(2)) // UNSUPPORTED
                        );

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        when(connection.getGoogleBottomBarIntentParams(any()))
                .thenReturn(
                        GoogleBottomBarIntentParams.newBuilder()
                                .setVariantLayoutType(
                                        VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                                .addAllEncodedButton(List.of(0, 2)) // SHARE
                                .build());
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100), // SAVE
                                createCustomActionButtonBundleWithId(101), // SHARE
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(2, dataProvider.getCustomButtonsOnGoogleBottomBar().size()); // SHARE, CUSTOM
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(2, dataProvider.getCustomButtonsOnToolbar().size()); // 1, SAVE
        assertEquals(4, dataProvider.getAllCustomButtons().size());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR})
    public void googleBottomBarFlagsOff_hasExtraGoogleBottomBarButtons_hasItemsInToolbar() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.shouldEnableGoogleBottomBarForIntent(any())).thenReturn(true);
        when(connection.hasExtraGoogleBottomBarButtons(any())).thenReturn(true);

        ArrayList<Bundle> googleBottomBarButtons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomGoogleBottomBarItemBundleWithId(103),
                                createCustomGoogleBottomBarItemBundleWithId(2)));

        when(connection.getGoogleBottomBarButtons(any())).thenReturn(googleBottomBarButtons);
        CustomTabsConnection.setInstanceForTesting(connection);

        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100),
                                createCustomActionButtonBundleWithId(1)));

        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(0, dataProvider.getCustomButtonsOnGoogleBottomBar().size());
        assertEquals(0, dataProvider.getCustomButtonsOnBottombar().size());
        assertEquals(2, dataProvider.getCustomButtonsOnToolbar().size());
        assertEquals(2, dataProvider.getAllCustomButtons().size());
    }

    @Test
    public void shareStateOn_buttonInToolbarAndCustomMenuItems_hasShareItemInMenu() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOff_noShareItems() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_OFF);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertTrue(dataProvider.getCustomButtonsOnToolbar().isEmpty());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES)
    public void isAllowedThirdParty_noDefaultPolicy() {
        ChromeFeatureList.sCctResizableForThirdPartiesDenylistEntries.setForTesting(
                "com.dc.joker|com.marvel.thanos");
        // If no default-policy is present, it defaults to use-denylist.
        assertFalse(
                "Entry in denylist should be rejected",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.dc.joker"));
        assertFalse(
                "Entry in denylist should be rejected",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.marvel.thanos"));
        assertTrue(
                "Entry NOT in denylist should be accepted",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.dc.batman"));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES
                    + ":default_policy/use-denylist"
                    + "/denylist_entries/com.dc.joker|com.marvel.thanos")
    public void isAllowedThirdParty_denylist() {
        ChromeFeatureList.sCctResizableForThirdPartiesDefaultPolicy.setForTesting("use-denylist");
        ChromeFeatureList.sCctResizableForThirdPartiesDenylistEntries.setForTesting(
                "com.dc.joker|com.marvel.thanos");
        assertFalse(
                "Entry in denylist should be rejected",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.dc.joker"));
        assertFalse(
                "Entry in denylist should be rejected",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.marvel.thanos"));
        assertTrue(
                "Entry NOT in denylist should be accepted",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.dc.batman"));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES
                    + ":default_policy/use-allowlist"
                    + "/allowlist_entries/com.pixar.woody|com.disney.ariel")
    public void isAllowedThirdParty_allowlist() {
        ChromeFeatureList.sCctResizableForThirdPartiesDefaultPolicy.setForTesting("use-allowlist");
        ChromeFeatureList.sCctResizableForThirdPartiesAllowlistEntries.setForTesting(
                "com.pixar.woody|com.disney.ariel");
        assertTrue(
                "Entry in allowlist should be accepted",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.pixar.woody"));
        assertTrue(
                "Entry in allowlist should be accepted",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.disney.ariel"));
        assertFalse(
                "Entry NOT in allowlist should be rejected",
                CustomTabIntentDataProvider.isAllowedThirdParty("com.pixar.syndrome"));
    }

    @Test
    public void testActivityBreakPoint_Default() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals("Break points do not match", 840, dataProvider.getActivityBreakPoint());
    }

    @Test
    public void testActivityBreakPoint_Custom() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, 300);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals("Break points do not match", 300, dataProvider.getActivityBreakPoint());
    }

    @Test
    public void testActivityBreakPoint_Negative() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, -500);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals("Break points do not match", 840, dataProvider.getActivityBreakPoint());
    }

    @Test
    public void testInitialActivityHeight_1stParty() {
        var intent = new Intent().putExtra(CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 50);
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.isFirstParty(any())).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(50, dataProvider.getInitialActivityHeight());
    }

    @Test
    public void testInitialActivityWidth_3P_notdenied() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 50);
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.pixar.woody");
        CustomTabsConnection.setInstanceForTesting(connection);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals("Width should be 50", 50, dataProvider.getInitialActivityWidth());
    }

    @Test
    public void testInitialActivityWidth_3P_denied() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 50);
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.dc.joker");
        CustomTabsConnection.setInstanceForTesting(connection);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        ChromeFeatureList.sCctResizableForThirdPartiesDenylistEntries.setForTesting(
                "com.dc.joker|com.marvel.thanos");
        assertEquals("Width should be 0", 0, dataProvider.getInitialActivityWidth());
    }

    @Test
    public void partialCustomTabHeightResizeBehavior_Default() {
        Intent intent =
                new Intent()
                        .putExtra(EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR, ACTIVITY_HEIGHT_DEFAULT);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertFalse(
                "The default height resize behavior should return false",
                dataProvider.isPartialCustomTabFixedHeight());
    }

    @Test
    public void partialCustomTabHeightResizeBehavior_Adjustable() {
        Intent intent =
                new Intent()
                        .putExtra(
                                EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR, ACTIVITY_HEIGHT_ADJUSTABLE);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertFalse(
                "The adjustable height resize behavior should return false",
                dataProvider.isPartialCustomTabFixedHeight());
    }

    @Test
    public void partialCustomTabHeightResizeBehavior_Fixed() {
        Intent intent =
                new Intent().putExtra(EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR, ACTIVITY_HEIGHT_FIXED);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertTrue(
                "The fixed height resize behavior should return true",
                dataProvider.isPartialCustomTabFixedHeight());
    }

    @Test
    public void partialCustomTabHeight_cornerRadius_defaultValue() {
        Intent intent = new Intent();

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        int defaultRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.custom_tabs_default_corner_radius);
        assertEquals(
                "Intent without the extra should have the default value.",
                defaultRadius,
                dataProvider.getPartialTabToolbarCornerRadius());
    }

    @Test
    public void partialCustomTabHeight_cornerRadius_intentExtra() {
        Intent intent = new Intent().putExtra(EXTRA_TOOLBAR_CORNER_RADIUS_DP, 0);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "The value from the extra should be returned.",
                0,
                dataProvider.getPartialTabToolbarCornerRadius());
    }

    @Test
    public void sideSheetSlideInBehavior() {
        // No extra
        var dataProvider =
                new CustomTabIntentDataProvider(new Intent(), mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should return ..SLIDE_IN_FROM_SIDE for the default slide-in behavior",
                ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE,
                dataProvider.getSideSheetSlideInBehavior());
    }

    @Test
    public void sideSheetPosition() {
        // No extra
        var dataProvider =
                new CustomTabIntentDataProvider(new Intent(), mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should return ..POSITION_END for the default side sheet position",
                ACTIVITY_SIDE_SHEET_POSITION_END,
                dataProvider.getSideSheetPosition());

        // Default
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_POSITION,
                                ACTIVITY_SIDE_SHEET_POSITION_DEFAULT);
        dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should return ..POSITION_END for the default side sheet position",
                ACTIVITY_SIDE_SHEET_POSITION_END,
                dataProvider.getSideSheetPosition());

        // Start
        intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_POSITION,
                                ACTIVITY_SIDE_SHEET_POSITION_START);
        dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should return ..POSITION_START",
                ACTIVITY_SIDE_SHEET_POSITION_START,
                dataProvider.getSideSheetPosition());

        // End
        intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_POSITION,
                                ACTIVITY_SIDE_SHEET_POSITION_END);
        dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should return ..POSITION_END",
                ACTIVITY_SIDE_SHEET_POSITION_END,
                dataProvider.getSideSheetPosition());
    }

    @Test
    public void testGetAppIdFromReferrer() {
        assertEquals(
                "extra.activity.referrer",
                CustomTabIntentDataProvider.getAppIdFromReferrer(
                        buildMockActivity("android-app://extra.activity.referrer")));
        assertEquals(
                "co.abc.xyz",
                CustomTabIntentDataProvider.getAppIdFromReferrer(
                        buildMockActivity("android-app://co.abc.xyz")));

        assertNonPackageUriReferrer("");
        assertNonPackageUriReferrer("invalid");
        assertNonPackageUriReferrer("android-app://"); // empty host name is invalid.
        assertNonPackageUriReferrer(Uri.parse("https://www.one.com").toString());
    }

    @Test
    public void testGetClientPackageName_Session() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.foo.bar");
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "com.baz.qux");
        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        Assert.assertEquals("com.foo.bar", dataProvider.getClientPackageName());
    }

    @Test
    public void testGetClientPackageName_Intent() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "com.foo.bar");
        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        Assert.assertEquals("com.foo.bar", dataProvider.getClientPackageName());
    }

    @Test
    public void testGetClientPackageName_None() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertNull(dataProvider.getClientPackageName());
    }

    @Test
    public void testGetClientPackageNameIdentitySharing() {
        Intent intent = new Intent();
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        Assert.assertNull(dataProvider.getClientPackageNameIdentitySharing());

        intent.putExtra(IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE, "com.foo.bar");
        var dataProvider2 = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        Assert.assertEquals("com.foo.bar", dataProvider2.getClientPackageNameIdentitySharing());
    }

    @Test
    public void testIsTrustedCustomTab_NoServiceConnection() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        when(connection.isFirstParty(eq("com.foo.bar"))).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        Assert.assertFalse(CustomTabIntentDataProvider.isTrustedCustomTab(intent, null));

        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "com.foo.bar");
        Assert.assertTrue(CustomTabIntentDataProvider.isTrustedCustomTab(intent, null));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateFeatureDisabled() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.example.foo");
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_AUTO_TRANSLATE_LANGUAGE, "es");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("fr", provider.getTranslateLanguage());
        assertFalse(provider.shouldAutoTranslate());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateExtraMissing() {
        ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.setForTesting(false);
        ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.setForTesting(
                "com.example.foo|com.example.bar");

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.example.foo");
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("fr", provider.getTranslateLanguage());
        assertFalse(provider.shouldAutoTranslate());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateWithAllowedPackageName() {
        ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.setForTesting(false);
        ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.setForTesting(
                "com.example.foo|com.example.bar");

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.example.foo");
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_AUTO_TRANSLATE_LANGUAGE, "es");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("es", provider.getTranslateLanguage());
        assertTrue(provider.shouldAutoTranslate());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateWithoutAllowedPackageName() {
        ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.setForTesting(false);
        ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.setForTesting(
                "com.example.foo|com.example.bar");

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.not.in.allowlist");
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_AUTO_TRANSLATE_LANGUAGE, "es");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("fr", provider.getTranslateLanguage());
        assertFalse(provider.shouldAutoTranslate());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateWithFirstPartyAllowed() {
        ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.setForTesting(true);
        ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.setForTesting(
                "com.example.foo|com.example.bar");

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.not.in.allowlist");
        when(connection.isFirstParty(eq("com.not.in.allowlist"))).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_AUTO_TRANSLATE_LANGUAGE, "es");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("es", provider.getTranslateLanguage());
        assertTrue(provider.shouldAutoTranslate());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_AUTO_TRANSLATE)
    public void getTranslateLanguage_autoTranslateWithThirdPartyPackageName() {
        ChromeFeatureList.sCctAutoTranslateAllowAllFirstParties.setForTesting(true);
        ChromeFeatureList.sCctAutoTranslatePackageNamesAllowlist.setForTesting(
                "com.example.foo|com.example.bar");

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.not.in.allowlist");
        when(connection.isFirstParty(eq("com.not.in.allowlist"))).thenReturn(false);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new Intent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_TRANSLATE_LANGUAGE, "fr");
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_AUTO_TRANSLATE_LANGUAGE, "es");
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals("fr", provider.getTranslateLanguage());
        assertFalse(provider.shouldAutoTranslate());
    }

    @Test
    public void getSecondaryToolbarSwipeUpPendingIntent() {
        Intent intent = new Intent();
        var pendingIntent = mock(PendingIntent.class);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION, pendingIntent);
        var provider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(pendingIntent, provider.getSecondaryToolbarSwipeUpPendingIntent());
    }

    @Test
    public void testCanInteractWithBackground() {
        Intent intent = new Intent();
        CustomTabIntentDataProvider provider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertTrue(
                "Background interaction should be enabled by default",
                provider.canInteractWithBackground());

        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_ENABLE_BACKGROUND_INTERACTION,
                BackgroundInteractBehavior.OFF);
        provider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(
                "Background interaction should be overridden by the legacy extra",
                provider.canInteractWithBackground());
    }

    @Test
    public void testActivityDecorationType_Default() {
        // Decoration not set
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Decoration types do not match",
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                dataProvider.getActivitySideSheetDecorationType());

        // Decoration set higher than max
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER + 1);
        var dataProvider2 = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Decoration types do not match",
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                dataProvider2.getActivitySideSheetDecorationType());
    }

    @Test
    public void testActivityDecorationType_Shadow() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Decoration types do not match",
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW,
                dataProvider.getActivitySideSheetDecorationType());
    }

    @Test
    public void testActivityDecorationType_DividerLine() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE,
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Decoration types do not match",
                ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER,
                dataProvider.getActivitySideSheetDecorationType());
    }

    @Test
    public void testActivityDecorationType_None() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE,
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Decoration types do not match",
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE,
                dataProvider.getActivitySideSheetDecorationType());
    }

    @Test
    public void testActivityRoundedCornersPosition_Default() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION,
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_DEFAULT);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Rounded corners positions do not match",
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE,
                dataProvider.getActivitySideSheetRoundedCornersPosition());
    }

    @Test
    public void testActivityRoundedCornersPosition_None() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION,
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Rounded corners positions do not match",
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE,
                dataProvider.getActivitySideSheetRoundedCornersPosition());
    }

    @Test
    public void testActivityRoundedCornersPosition_Top() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION,
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Rounded corners positions do not match",
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP,
                dataProvider.getActivitySideSheetRoundedCornersPosition());
    }

    @Test
    public void testActivityScrollContentResize() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_ACTIVITY_SCROLL_CONTENT_RESIZE, true);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue("scroll-content-resize was not set", dataProvider.contentScrollMayResizeTab());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void isInteractiveOmniboxEnabled_flagEnabled() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");
        assertTrue(dataProvider.isInteractiveOmniboxEnabled());

        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        assertTrue(dataProvider.isInteractiveOmniboxEnabled());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void isInteractiveOmniboxEnabled_flagDisabled() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");
        assertFalse(dataProvider.isInteractiveOmniboxEnabled());

        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        assertFalse(dataProvider.isInteractiveOmniboxEnabled());
    }

    @Test
    public void searchInCct_notAllowedInOffTheRecordMode() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider =
                spy(new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT));
        when(dataProvider.isOffTheRecord()).thenReturn(true);

        assertFalse(dataProvider.isInteractiveOmniboxAllowed());
    }

    @Test
    public void searchInCct_notAllowedOnPartialCcts() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider =
                spy(new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT));
        when(dataProvider.isPartialCustomTab()).thenReturn(true);

        assertFalse(dataProvider.isInteractiveOmniboxAllowed());
    }

    @Test
    public void searchInCct_notAllowedOnAutomotive() {
        var shadowPkgMgr = Shadows.shadowOf(mContext.getPackageManager());
        shadowPkgMgr.setSystemFeature(PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        assertTrue(DeviceInfo.isAutomotive());

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertFalse(dataProvider.isInteractiveOmniboxAllowed());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addShareOption_conventionalCct_defaultState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(false);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the share button to be created.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        var button = buttons.get(0);
        assertEquals(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON, button.getType());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addShareOption_conventionalCct_disabledState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(false);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_OFF);

        // Buttons are initialized as part of the Constructor logic.
        // Expect no toolbar buttons.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(0, buttons.size());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addShareOption_searchInCct_enabledState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(true);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(
                CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, buttons.get(0).getType());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addOpenInBrowserOption_searchInCct_defaultState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(true);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;

        // Buttons are initialized as part of the Constructor logic.
        // Since we're simulating an Omnibox-enabled CCT, expect the default button to be the Open
        // in Browser.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        var button = buttons.get(0);
        assertEquals(CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, button.getType());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addOpenInBrowserOption_searchInCct_disabledState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(true);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_OFF);

        // Buttons are initialized as part of the Constructor logic.
        // Since we explicitly disabled the Open in Browser button, we should pick the implicitly
        // added Share button.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        var button = buttons.get(0);
        assertEquals(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON, button.getType());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEARCH_IN_CCT})
    public void addOpenInBrowserOption_conventionalCct_enabledState() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        when(connection.shouldEnableOmniboxForIntent(any())).thenReturn(false);
        when(connection.getClientPackageNameForSession(any())).thenReturn("com.a.b.c");

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(
                CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, buttons.get(0).getType());
    }

    @Test
    public void openInBrowserStateExtraTrue_enabledByEmbedderTrue_openInBrowserButtonAdded() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(
                CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, buttons.get(0).getType());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER})
    @EnableFeatures({ChromeFeatureList.CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER})
    public void
            openInBrowserStateExtraTrue_enabledByEmbedderFalse_allowedByEmbedderTrue_openInBrowserButtonAllowedExtraTrue_openInBrowserButtonAdded() {
        Intent intent = addExtrasForOpenInBrowserButton(true);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(
                CustomButtonParams.ButtonType.CCT_OPEN_IN_BROWSER_BUTTON, buttons.get(0).getType());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER})
    public void
            openInBrowserStateExtraTrue_enabledByEmbedderFalse_allowedByEmbedderFalse_openInBrowserButtonAllowedExtraTrue_openInBrowserButtonNotAdded() {
        Intent intent = addExtrasForOpenInBrowserButton(true);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON, buttons.get(0).getType());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER})
    @EnableFeatures({ChromeFeatureList.CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER})
    public void
            openInBrowserStateExtraTrue_enabledByEmbedderFalse_allowedByEmbedderTrue_openInBrowserButtonAllowedExtraFalse_openInBrowserButtonNotAdded() {
        Intent intent = addExtrasForOpenInBrowserButton(false);

        // Buttons are initialized as part of the Constructor logic.
        // Expect only the Open in Browser button, as the Share button is gated by empty Toolbar.
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals(1, buttons.size());
        assertEquals(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON, buttons.get(0).getType());
    }

    private Intent addExtrasForOpenInBrowserButton(boolean openInBrowserButtonAllowedExtra) {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_BUTTON_ALLOWED,
                openInBrowserButtonAllowedExtra);
        return intent;
    }

    private Bundle createActionButtonInToolbarBundle() {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);
        int iconHeight = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        bundle.putParcelable(
                CustomTabsIntent.KEY_ICON,
                Bitmap.createBitmap(iconHeight, iconHeight, Bitmap.Config.ALPHA_8));
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, BUTTON_DESCRIPTION);
        bundle.putParcelable(
                CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(
                        mContext,
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    private Bundle createCustomActionButtonBundleWithId(int id) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        int iconHeight = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        bundle.putParcelable(
                CustomTabsIntent.KEY_ICON,
                Bitmap.createBitmap(iconHeight, iconHeight, Bitmap.Config.ALPHA_8));
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, BUTTON_DESCRIPTION);
        bundle.putParcelable(
                CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(
                        mContext,
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    private Bundle createCustomGoogleBottomBarItemBundleWithId(int id) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        int iconHeight = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        bundle.putParcelable(
                CustomTabsIntent.KEY_ICON,
                Bitmap.createBitmap(iconHeight, iconHeight, Bitmap.Config.ALPHA_8));
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, BUTTON_DESCRIPTION);
        bundle.putParcelable(
                CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(
                        mContext,
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, false);
        return bundle;
    }

    private Bundle createMenuItemBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, "title");
        bundle.putParcelable(
                CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(
                        mContext,
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        return bundle;
    }

    private ArrayList<Uri> getSampleUriList() {
        return new ArrayList<>(
                Arrays.asList(
                        Uri.parse("content://com.a.b.c/a"), Uri.parse("content://com.a.b.c/b")));
    }

    private Bundle createFileHandlingDataBundle() {
        Bundle bundle = new Bundle();
        bundle.putParcelableArrayList(FileHandlingData.KEY_URIS, getSampleUriList());
        return bundle;
    }

    protected Uri getLaunchingUrl() {
        return Uri.parse("https://www.example.com/");
    }

    private void assertNonPackageUriReferrer(String referrerStr) {
        assertEquals(
                referrerStr,
                CustomTabIntentDataProvider.getAppIdFromReferrer(buildMockActivity(referrerStr)));
    }

    private Activity buildMockActivity(String referrer) {
        Activity mockActivity = Mockito.mock(Activity.class);
        Mockito.doReturn(new Intent()).when(mockActivity).getIntent();
        Mockito.doReturn(Uri.parse(referrer)).when(mockActivity).getReferrer();
        return mockActivity;
    }

    @Test
    public void requestUiType_withTargetNetwork() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);
        Network network = Mockito.mock(Network.class);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabsIntent.EXTRA_NETWORK,
                network);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabsUiType.NETWORK_BOUND_TAB);

        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(CustomTabsUiType.NETWORK_BOUND_TAB, dataProvider.getUiType());
    }

    @Test
    public void requestUiType_withoutTargetNetwork() {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabsUiType.NETWORK_BOUND_TAB);

        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(CustomTabsUiType.DEFAULT, dataProvider.getUiType());
    }

    @Test
    public void setCloseButtonDisabled() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED, false);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(dataProvider.isCloseButtonEnabled());
    }

    @Test
    public void setCloseButtonEnabled() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED, true);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue(dataProvider.isCloseButtonEnabled());
    }

    @Test
    public void testCloseButtonDisabled_disablesCloseButtonCustomization() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        Bitmap icon =
                Bitmap.createBitmap(/* width= */ 16, /* height= */ 16, Bitmap.Config.ARGB_8888);
        intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED, false);
        intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, icon);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(dataProvider.isCloseButtonEnabled());
        assertNull(dataProvider.getCloseButtonDrawable());
        assertEquals(
                CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT,
                dataProvider.getCloseButtonPosition());
    }

    @Test
    public void testCloseButtonEnabledByDefault_enablesCloseButtonCustomization() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        Bitmap icon =
                Bitmap.createBitmap(/* width= */ 16, /* height= */ 16, Bitmap.Config.ARGB_8888);
        intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, icon);
        intent.putExtra(
                CustomTabsIntent.EXTRA_CLOSE_BUTTON_POSITION,
                CustomTabsIntent.CLOSE_BUTTON_POSITION_END);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue(dataProvider.isCloseButtonEnabled());
        assertNotNull(dataProvider.getCloseButtonDrawable());
        assertEquals(
                CustomTabsIntent.CLOSE_BUTTON_POSITION_END, dataProvider.getCloseButtonPosition());
    }

    @Test
    public void launchHandlerClientMode() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_LAUNCH_HANDLER_CLIENT_MODE,
                LaunchHandlerClientMode.FOCUS_EXISTING);

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                LaunchHandlerClientMode.FOCUS_EXISTING, dataProvider.getLaunchHandlerClientMode());
    }

    @Test
    public void launchHandlerClientMode_noValue() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(LaunchHandlerClientMode.AUTO, dataProvider.getLaunchHandlerClientMode());
    }

    @Test
    public void launchHandlerClientMode_wrongValue() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_LAUNCH_HANDLER_CLIENT_MODE, 45);

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(LaunchHandlerClientMode.AUTO, dataProvider.getLaunchHandlerClientMode());
    }

    @Test
    public void launchHandlerFileHandlingData() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA,
                createFileHandlingDataBundle());

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        FileHandlingData data = dataProvider.getFileHandlingData();
        assertEquals(getSampleUriList(), data.uris);
    }

    @Test
    public void testTwaFullscreenDisplayMode_ResolveToFullscreen() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.ImmersiveMode(
                        false, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT),
                null,
                DisplayMode.FULLSCREEN);
    }

    @Test
    public void testTwaStandaloneDisplayMode_ResolveToStandalone() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.DefaultMode(), null, DisplayMode.STANDALONE);
    }

    @Test
    public void testTwaStandaloneDisplayOverride_ResolveToImmersive() {
        checkResolvedDisplayMode(
                null,
                Collections.singletonList(
                        new TrustedWebActivityDisplayMode.ImmersiveMode(
                                false,
                                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)),
                DisplayMode.FULLSCREEN);
    }

    @Test
    public void testTwaStandaloneDisplayOverride_BrowserIgnored() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.DefaultMode(),
                Collections.singletonList(new TrustedWebActivityDisplayMode.BrowserMode()),
                DisplayMode.STANDALONE);
    }

    @Test
    public void testTwaStandaloneDisplayOverride_OverridePrioritized() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.DefaultMode(),
                Collections.singletonList(
                        new TrustedWebActivityDisplayMode.ImmersiveMode(
                                false,
                                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT)),
                DisplayMode.FULLSCREEN);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaMinUiEnabledDisplayMode_ResolveToMinimalUi() {
        // on sdk < 35 min ui is not supported
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.MinimalUiMode(), null, DisplayMode.STANDALONE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @DisableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaMinUiDisabledDisplayMode_ResolveToStandalone() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.MinimalUiMode(), null, DisplayMode.STANDALONE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaBrowserModeWithEnabledMinUI_ResolveToMinimalUi() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.BrowserMode(), null, DisplayMode.MINIMAL_UI);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaBrowserModeWithEnabledMinUI_ResolveDisplayOverrideToMinimalUi() {
        checkResolvedDisplayMode(
                null,
                Collections.singletonList(new TrustedWebActivityDisplayMode.MinimalUiMode()),
                DisplayMode.MINIMAL_UI);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaBrowserModeWithEnabledMinUiPreSdk35_ResolveToMinimalUi() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.BrowserMode(), null, DisplayMode.STANDALONE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @DisableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testTwaBrowserModeWithDisabledMinimalUi_ResolveToStandalone() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.BrowserMode(), null, DisplayMode.STANDALONE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @DisableFeatures({ChromeFeatureList.ANDROID_WINDOW_CONTROLS_OVERLAY})
    public void testTwaBrowserModeWithDisabledWindowControlsOverlay_ResolveToStandalone() {
        checkResolvedDisplayMode(
                null,
                Collections.singletonList(
                        new TrustedWebActivityDisplayMode.WindowControlsOverlayMode()),
                DisplayMode.STANDALONE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_WINDOW_CONTROLS_OVERLAY})
    public void testTwaBrowserModeWithEnableWindowControlsOverlay_ResolveToWindowControlsOverlay() {
        checkResolvedDisplayMode(
                null,
                Collections.singletonList(
                        new TrustedWebActivityDisplayMode.WindowControlsOverlayMode()),
                DisplayMode.WINDOW_CONTROLS_OVERLAY);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_WINDOW_CONTROLS_OVERLAY})
    public void
            testTwaBrowserModeWithEnableWindowControlsOverlay_IgnoreWindowControlsOverlayNotInDisplayOverride() {
        checkResolvedDisplayMode(
                new TrustedWebActivityDisplayMode.WindowControlsOverlayMode(),
                null,
                DisplayMode.STANDALONE);
    }

    private void checkResolvedDisplayMode(
            @Nullable TrustedWebActivityDisplayMode displayMode,
            @Nullable List<TrustedWebActivityDisplayMode> displayOverrides,
            @DisplayMode.EnumType int expectedDisplayMode) {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);

        if (displayMode != null) {
            intent.putExtra(
                    TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE, displayMode.toBundle());
        }
        if (displayOverrides != null) {
            ArrayList<Bundle> bundles = new ArrayList<>();
            for (TrustedWebActivityDisplayMode mode : displayOverrides) {
                bundles.add(mode.toBundle());
            }
            intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_OVERRIDE, bundles);
        }

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Display mode resolution mismatch",
                expectedDisplayMode,
                dataProvider.getResolvedDisplayMode());
    }

    @Test
    public void testResolveTwaDisplayModeNotForTwa_ResolveToUndefined() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should resolve to undefined display mode",
                DisplayMode.UNDEFINED,
                dataProvider.getResolvedDisplayMode());
    }

    @Test
    public void uiTypePopup_hasNoToolbarButtons() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED, true)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        // If there are no custom buttons defined, then the share button is added to the set of
        // custom toolbar buttons. Otherwise it gets punted to menu.
        // The open in browser button can be presented only by being added to the set of custom
        // toolbar buttons.
        assertEquals(
                "There should be no buttons on toolbar",
                0,
                dataProvider.getCustomButtonsOnToolbar().size());

        assertFalse("The close button should be disabled", dataProvider.isCloseButtonEnabled());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypePopup_hasNoToolbarButtons_incognitoCct() {
        final Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ENABLED, true)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        final IncognitoCustomTabIntentDataProvider dataProvider =
                new IncognitoCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        // If there are no custom buttons defined, then the share button is added to the set of
        // custom toolbar buttons. Otherwise it gets punted to menu.
        // The open in browser button can be presented only by being added to the set of custom
        // toolbar buttons.
        assertEquals(
                "There should be no buttons on toolbar",
                0,
                dataProvider.getCustomButtonsOnToolbar().size());

        assertFalse("The close button should be disabled", dataProvider.isCloseButtonEnabled());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures(ChromeFeatureList.ANDROID_WEB_APP_MENU_BUTTON)
    public void uiTypeTwa_withExperimentFlag_returnsWebAppMenu() {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should resolve to minimal ui display mode",
                DisplayMode.MINIMAL_UI,
                dataProvider.getResolvedDisplayMode());
        assertEquals(
                "Should resolve to TRUSTED_WEB_ACTIVITY",
                CustomTabsUiType.TRUSTED_WEB_ACTIVITY,
                dataProvider.getUiType());
    }

    @Test
    public void testGetOpenInBrowserButtonState_defaultState() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT);

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should resolve to the default state",
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT,
                dataProvider.getOpenInBrowserButtonState());
    }

    @Test
    public void testGetOpenInBrowserButtonState_notSet() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "Should resolve to the default state",
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT,
                dataProvider.getOpenInBrowserButtonState());
    }

    @Test
    public void testGetOpenInBrowserButtonState_offState() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF);

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should resolve to the off state",
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF,
                dataProvider.getOpenInBrowserButtonState());
    }

    @Test
    public void testGetOpenInBrowserButtonState_onState() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON);

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "Should resolve to the on state",
                CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON,
                dataProvider.getOpenInBrowserButtonState());
    }

    @Test
    public void testOpenInBrowser_customButtonsOverOIBOn() {
        // 2 Custom buttons + OIB on -> 2 custom buttons, OIB ignored.
        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100),
                                createCustomActionButtonBundleWithId(1)));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", 100, actionButtons.get(0).getId());
        assertEquals("Custom action ID (1).", 1, actionButtons.get(1).getId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testGetCustomConentActions_noneDefined() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue("Should return an empty list", dataProvider.getCustomContentActions().isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_CONTEXTUAL_MENU_ITEMS)
    public void testGetCustomContentActions_returnsListOfCustomContentAction_whenDefined() {
        int id = 1;
        String label = "Pin Image";
        var pendingIntent = mock(PendingIntent.class);
        @CustomTabsIntent.ContentTargetType
        int targetType = CustomTabsIntent.CONTENT_TARGET_TYPE_IMAGE;

        CustomContentAction action =
                new CustomContentAction.Builder(id, label, pendingIntent, targetType).build();

        Intent intent =
                new CustomTabsIntent.Builder().addCustomContentAction(action).build().intent;

        BrowserServicesIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(
                "There should be one custom content action in the returned list.",
                1,
                dataProvider.getCustomContentActions().size());
        assertEquals(
                "The id of the one and only custom content action should be == id (1).",
                id,
                dataProvider.getCustomContentActions().get(0).getId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported_featureEnabled() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue(
                "Normal CCT should support optional button",
                dataProvider.isOptionalButtonSupported());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported_featureDisabled() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(
                "Should not support optional button if feature is disabled",
                dataProvider.isOptionalButtonSupported());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported_trustedWebActivity() {
        CustomTabsSession mSession =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        var twaBuilder = new TrustedWebActivityIntentBuilder(getLaunchingUrl());
        Intent intent = twaBuilder.build(mSession).getIntent();
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertTrue("IntentDataProvider should be for TWA", dataProvider.isTrustedWebActivity());
        assertFalse(
                "TWA should NOT support optional button", dataProvider.isOptionalButtonSupported());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported_ephemeralCct() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        var dataProvider =
                new EphemeralCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(
                "eCCT should NOT support optional button",
                dataProvider.isOptionalButtonSupported());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported_incognitoCct() {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.setData(Uri.parse("http://www.example.com"));
        var dataProvider =
                new IncognitoCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertFalse(
                "iCCT should NOT support optional button",
                dataProvider.isOptionalButtonSupported());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testCustomActionButtonsLimitUpTo2() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(
                        Arrays.asList(
                                createCustomActionButtonBundleWithId(100), // SAVE
                                createCustomActionButtonBundleWithId(101), // SHARE
                                createCustomActionButtonBundleWithId(1)));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_OFF)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertTrue(
                "Custom/Chrome action button count should not exceed 2",
                dataProvider.getCustomButtonsOnToolbar().size() <= 2);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_noCustomAction() {
        // No custom action + all default -> Share
        // The other slot available for MTB/CPA, showing OIB as default
        Intent intent = new Intent();
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var buttons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be a single action.", 1, buttons.size());
        assertEquals("Chrome share action", ButtonType.CCT_SHARE_BUTTON, buttons.get(0).getType());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_customAction() {
        // Custom action -> custom action
        // The other slot available for MTB/CPA, showing OIB as default
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId)); // One custom action
        Intent intent = new Intent().putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be a single action.", 1, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_customAction_oib() {
        // Custom action + OIB on -> custom action + OIB
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId)); // One custom action
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
        assertEquals(
                "Chrome OIB.",
                ButtonType.CCT_OPEN_IN_BROWSER_BUTTON,
                actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_customAction_share() {
        // Custom action + share on -> custom action + share
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId)); // One custom action
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_customAction_share_oib() {
        // Custom + Share on + OIB on -> Custom + Share
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId));
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CCT_ADAPTIVE_BUTTON
                    + ":open_in_browser/true/default_variant/15/contextual_only/true")
    public void testMtbCct_CpaOib_Share_Oib() {
        // Share on + OIB on -> Share + OIB
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals(
                "Chrome OIB.",
                ButtonType.CCT_OPEN_IN_BROWSER_BUTTON,
                actionButtons.get(0).getType());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testMtbCct_otherConfig_customAction() {
        // Custom -> Custom
        // The other slot available for MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId)); // One custom action.
        Intent intent = new Intent().putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be a single action.", 1, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testMtbCct_otherConfig_customAction_Oib() {
        // Custom action + OIB on -> custom action + OIB
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId));
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
        assertEquals(
                "Chrome OIB.",
                ButtonType.CCT_OPEN_IN_BROWSER_BUTTON,
                actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testMtbCct_otherConfig_customAction_Share() {
        // Custom action + share on -> custom action + share
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId));
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testMtbCct_otherConfig_customAction_Share_Oib() {
        // Custom action + share on + OIB on -> custom action + share
        // No MTB/CPA
        int customId = 100;
        var buttons = new ArrayList<Bundle>();
        buttons.add(createCustomActionButtonBundleWithId(customId));
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals("Custom action ID (100).", customId, actionButtons.get(0).getId());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testMtbCct_otherConfig_Share_Oib() {
        // Share on + OIB on -> share + OIB
        // No MTB/CPA
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_OPEN_IN_BROWSER_STATE,
                                CustomTabIntentDataProvider.CustomTabsButtonState.BUTTON_STATE_ON);
        var dataProvider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        var actionButtons = dataProvider.getCustomButtonsOnToolbar();
        assertEquals("There should be 2 actions.", 2, actionButtons.size());
        assertEquals(
                "Chrome OIB.",
                ButtonType.CCT_OPEN_IN_BROWSER_BUTTON,
                actionButtons.get(0).getType());
        assertEquals("Chrome Share.", ButtonType.CCT_SHARE_BUTTON, actionButtons.get(1).getType());
    }

    @Test
    public void uiTypePopup_returnsRequestedWindowFeatures() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        Intent intent =
                new Intent()
                        .putExtra(
                                PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES,
                                windowFeatures.toBundle())
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "The data provider has not returned the window features specified in the intent",
                windowFeatures,
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypePopup_returnsRequestedWindowFeatures_incognitoCct() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        final Intent intent =
                new Intent()
                        .putExtra(
                                PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES,
                                windowFeatures.toBundle())
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        final IncognitoCustomTabIntentDataProvider dataProvider =
                new IncognitoCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "The data provider has not returned the window features specified in the intent",
                windowFeatures,
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypePopup_returnsEmptyWindowFeaturesWhenNotSpecifiedInIntent() {
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "The data provider has not returned empty window features",
                new WindowFeatures(),
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypePopup_returnsEmptyWindowFeaturesWhenNotSpecifiedInIntent_incognitoCct() {
        final Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        final IncognitoCustomTabIntentDataProvider dataProvider =
                new IncognitoCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(
                "The data provider has not returned empty window features",
                new WindowFeatures(),
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypeDefault_returnsNullRequestedWindowFeatures() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        Intent intent =
                new Intent()
                        .putExtra(
                                PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES,
                                windowFeatures.toBundle())
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                                CustomTabsUiType.DEFAULT);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertNull(
                "The data provider has returned the window features specified in the intent even if"
                        + " the UI type is not popup",
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypeDefault_returnsNullRequestedWindowFeatures_incognitoCct() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        final Intent intent =
                new Intent()
                        .putExtra(
                                PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES,
                                windowFeatures.toBundle())
                        .putExtra(
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                                CustomTabsUiType.DEFAULT);
        IntentUtils.setForceIsTrustedIntentForTesting(true);

        final IncognitoCustomTabIntentDataProvider dataProvider =
                new IncognitoCustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertNull(
                "The data provider has returned the window features specified in the intent even if"
                        + " the UI type is not popup",
                dataProvider.getRequestedWindowFeatures());

        IntentUtils.setForceIsTrustedIntentForTesting(false);
    }

    @Test
    public void uiTypes_openInBrowserButtonState() {
        final int stateDefault = CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT;
        final int stateOff = CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;

        assertEquals(stateDefault, getOibStateForType(CustomTabsUiType.DEFAULT));

        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.NETWORK_BOUND_TAB));
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.AUTH_TAB));
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.MEDIA_VIEWER));
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.POPUP));
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.READER_MODE));
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.OFFLINE_PAGE));
    }

    @Test
    public void uiTypes_openInBrowserButtonState_firstRunStatus() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        final int stateOff = CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;

        // Without completing first run, OIB won't be shown.
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.DEFAULT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WEB_APP_MENU_BUTTON)
    public void uiTypes_openInBrowserButtonState_twa() {
        final int stateOff = CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;
        assertEquals(stateOff, getOibStateForType(CustomTabsUiType.TRUSTED_WEB_ACTIVITY));
    }

    private int getOibStateForType(int type) {
        if (type == CustomTabsUiType.AUTH_TAB) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse("https://www.google.com"));
            intent.putExtra(AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, true);
            intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, PACKAGE);
            Bundle bundle = new Bundle();
            bundle.putBinder(CustomTabsIntent.EXTRA_SESSION, null);
            intent.putExtras(bundle);

            var provider = new AuthTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
            return provider.getOpenInBrowserButtonState();

        } else {
            Intent intent = new Intent().putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, type);
            if (type == CustomTabsUiType.NETWORK_BOUND_TAB) {
                Network network = Mockito.mock(Network.class);
                intent.putExtra(CustomTabsIntent.EXTRA_NETWORK, network);
            }
            setIsTrustedCustomTab(intent);
            var provider = new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
            return provider.getOpenInBrowserButtonState();
        }
    }

    private static void setIsTrustedCustomTab(Intent intent) {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        when(connection.getClientPackageNameForSession(any())).thenReturn(null);
        when(connection.isFirstParty(eq(PACKAGE))).thenReturn(true);
        CustomTabsConnection.setInstanceForTesting(connection);
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, PACKAGE);
    }
}
