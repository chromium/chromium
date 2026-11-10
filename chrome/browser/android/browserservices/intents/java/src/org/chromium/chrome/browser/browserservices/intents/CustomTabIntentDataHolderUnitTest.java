// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Network;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.util.Pair;
import android.view.WindowManager;
import android.widget.RemoteViews;

import androidx.browser.auth.AuthTabColorSchemeParams;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.DefaultMode;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TitleVisibility;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomButtonParamsImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.components.browser_ui.widget.TintedDrawable;

import java.util.List;

/** Unit tests for {@link CustomTabIntentDataHolder} and {@link SessionHolder} serialization. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabIntentDataHolderUnitTest {
    private static CustomTabIntentDataHolder roundTripThroughParcel(
            CustomTabIntentDataHolder holder) {
        Parcel parcel = Parcel.obtain();
        try {
            holder.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return CustomTabIntentDataHolder.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }

    @Test
    public void testParcelRoundTrip_defaultValues() {
        CustomTabIntentDataHolder original = new CustomTabIntentDataHolder.Builder().build();
        CustomTabIntentDataHolder restored = roundTripThroughParcel(original);

        assertNull(restored.mSessionHolder);
        assertNull(restored.mClientPackageName);
        assertFalse(restored.mIsTrustedIntent);
        assertNull(restored.mAnimationBundle);
        assertNull(restored.mKeepAliveServiceIntent);
        assertNull(restored.mNetwork);
        assertFalse(restored.mIsOpenedByChrome);
        assertEquals(0, restored.mUiType);
        assertEquals(0, restored.mTitleVisibilityState);
        assertEquals(0, restored.mInitialActivityHeight);
        assertEquals(0, restored.mInitialActivityWidth);
        assertEquals(0, restored.mBreakPointDp);
        assertEquals(0, restored.mPartialTabToolbarCornerRadius);
        assertEquals(0, restored.mActivityType);
        assertEquals(CustomTabProfileType.REGULAR, restored.mCustomTabMode);
        assertNull(restored.mMediaViewerUrl);
        assertFalse(restored.mEnableEmbeddedMediaExperience);
        assertFalse(restored.mIsFromMediaLauncherActivity);
        assertFalse(restored.mDisableStar);
        assertFalse(restored.mDisableDownload);
        assertNull(restored.mTwaAdditionalOrigins);
        assertNull(restored.mTwaDisplayMode);
        assertTrue(restored.mTwaDisplayOverrideMode.isEmpty());
        assertFalse(restored.mEnableUrlBarHiding);
        assertFalse(restored.mIsCloseButtonEnabled);
        assertNull(restored.mCloseButtonIcon);
        assertNull(restored.mRemoteViews);
        assertEquals(0, restored.mSideSheetDecorationType);
        assertEquals(0, restored.mSideSheetRoundedCornersPosition);
        assertNull(restored.mClickableViewIds);
        assertNull(restored.mRemoteViewsPendingIntent);
        assertNull(restored.mSecondaryToolbarSwipeUpPendingIntent);
        assertEquals(0, restored.mOpenInBrowserState);
        assertNull(restored.mTranslateLanguage);
        assertNull(restored.mAutoTranslateLanguage);
        assertEquals(0, restored.mDefaultOrientation);
        assertNull(restored.mGsaExperimentIds);
        assertFalse(restored.mIsPartialCustomTabFixedHeight);
        assertFalse(restored.mContentScrollMayResizeTab);
        assertFalse(restored.mInteractWithBackground);
        assertFalse(restored.mTabbedCctAllowedByEmbedder);
        assertFalse(restored.mTabbedCctEnabledByEmbedder);
        assertNull(restored.mAuthRedirectScheme);
        assertNull(restored.mAuthRedirectHost);
        assertNull(restored.mAuthRedirectPath);
        assertNull(restored.mLightColorSchemeParams.toolbarColor);
        assertNull(restored.mLightColorSchemeParams.secondaryToolbarColor);
        assertNull(restored.mLightColorSchemeParams.navigationBarColor);
        assertNull(restored.mLightColorSchemeParams.navigationBarDividerColor);
        assertNull(restored.mDarkColorSchemeParams.toolbarColor);
        assertNull(restored.mDarkColorSchemeParams.secondaryToolbarColor);
        assertNull(restored.mDarkColorSchemeParams.navigationBarColor);
        assertNull(restored.mDarkColorSchemeParams.navigationBarDividerColor);
        assertNull(restored.mLightAuthTabColorSchemeParams.getToolbarColor());
        assertNull(restored.mLightAuthTabColorSchemeParams.getNavigationBarColor());
        assertNull(restored.mLightAuthTabColorSchemeParams.getNavigationBarDividerColor());
        assertNull(restored.mDarkAuthTabColorSchemeParams.getToolbarColor());
        assertNull(restored.mDarkAuthTabColorSchemeParams.getNavigationBarColor());
        assertNull(restored.mDarkAuthTabColorSchemeParams.getNavigationBarDividerColor());
        assertEquals(Color.TRANSPARENT, restored.mInitialBackgroundColor);
        assertTrue(restored.mCustomButtonParams.isEmpty());
        assertEquals(CustomTabsIntent.SHARE_STATE_DEFAULT, restored.mShareState);
        assertFalse(restored.mDefaultShareMenuItem);
        assertFalse(restored.mOpenInBrowserButtonAllowed);
        assertTrue(restored.mMenuEntries.isEmpty());
        assertSame(
                restored.mLightColorSchemeParams,
                restored.getColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_LIGHT));
        assertSame(
                restored.mDarkColorSchemeParams,
                restored.getColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_DARK));
        assertSame(
                restored.mLightAuthTabColorSchemeParams,
                restored.getAuthTabColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_LIGHT));
        assertSame(
                restored.mDarkAuthTabColorSchemeParams,
                restored.getAuthTabColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_DARK));
    }

    @Test
    public void testParcelRoundTrip_populatedValues() {
        Context context = ContextUtils.getApplicationContext();

        IBinder binder = new Binder();
        PendingIntent sessionId =
                PendingIntent.getActivity(
                        context, 1, new Intent("session_id"), PendingIntent.FLAG_IMMUTABLE);
        Intent sessionIntent = new Intent();
        Bundle sessionExtras = new Bundle();
        sessionExtras.putBinder(CustomTabsIntent.EXTRA_SESSION, binder);
        sessionExtras.putParcelable(CustomTabsIntent.EXTRA_SESSION_ID, sessionId);
        sessionIntent.putExtras(sessionExtras);
        SessionHolder sessionHolder = SessionHolder.getSessionHolderFromIntent(sessionIntent);
        assertNotNull(sessionHolder);
        assertTrue(sessionHolder.isCustomTab());

        Bundle animationBundle = new Bundle();
        animationBundle.putString("package", "com.example.client");
        animationBundle.putInt("enter", 10);
        animationBundle.putInt("exit", 20);

        Intent keepAliveIntent = new Intent("com.example.KEEP_ALIVE");
        Parcel networkParcel = Parcel.obtain();
        networkParcel.writeInt(12345);
        networkParcel.setDataPosition(0);
        Network network = Network.CREATOR.createFromParcel(networkParcel);
        networkParcel.recycle();
        Bitmap closeButtonIcon = Bitmap.createBitmap(16, 16, Bitmap.Config.ARGB_8888);
        RemoteViews remoteViews =
                new RemoteViews(context.getPackageName(), android.R.layout.list_content);
        PendingIntent remoteViewsPi =
                PendingIntent.getBroadcast(
                        context, 2, new Intent("remote_views"), PendingIntent.FLAG_IMMUTABLE);
        PendingIntent swipeUpPi =
                PendingIntent.getBroadcast(
                        context, 3, new Intent("swipe_up"), PendingIntent.FLAG_IMMUTABLE);
        PendingIntent menuPi =
                PendingIntent.getActivity(
                        context, 7, new Intent("menu_1"), PendingIntent.FLAG_IMMUTABLE);
        PendingIntent menuPi2 =
                PendingIntent.getActivity(
                        context, 8, new Intent("menu_2"), PendingIntent.FLAG_IMMUTABLE);
        CustomButtonParams shareButton =
                CustomButtonParamsImpl.createShareButton(context, Color.BLACK);
        CustomButtonParams bottomBarButton =
                CustomButtonParamsImpl.fromIntent(
                                context,
                                new CustomTabsIntent.Builder()
                                        .addToolbarItem(
                                                8,
                                                Bitmap.createBitmap(8, 8, Bitmap.Config.ARGB_8888),
                                                "eight",
                                                swipeUpPi)
                                        .build()
                                        .intent)
                        .get(0);

        ImmersiveMode twaDisplayMode =
                new ImmersiveMode(
                        /* isSticky= */ true,
                        WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        CustomTabIntentDataHolder original =
                new CustomTabIntentDataHolder.Builder()
                        .setSessionHolder(sessionHolder)
                        .setClientPackageName("com.example.client")
                        .setIsTrustedIntent(true)
                        .setAnimationBundle(animationBundle)
                        .setKeepAliveServiceIntent(keepAliveIntent)
                        .setNetwork(network)
                        .setIsOpenedByChrome(true)
                        .setUiType(CustomTabsUiType.MEDIA_VIEWER)
                        .setTitleVisibilityState(TitleVisibility.VISIBLE)
                        .setInitialActivityHeight(500)
                        .setInitialActivityWidth(400)
                        .setBreakPointDp(600)
                        .setPartialTabToolbarCornerRadius(16)
                        .setActivityType(ActivityType.TRUSTED_WEB_ACTIVITY)
                        .setCustomTabMode(CustomTabProfileType.EPHEMERAL)
                        .setMediaViewerUrl("file:///sdcard/test.mp4")
                        .setEnableEmbeddedMediaExperience(true)
                        .setIsFromMediaLauncherActivity(true)
                        .setDisableStar(true)
                        .setDisableDownload(true)
                        .setTwaAdditionalOrigins(
                                List.of("https://a.example.com", "https://b.example.com"))
                        .setTwaDisplayMode(twaDisplayMode)
                        .setTwaDisplayOverrideMode(List.of(new DefaultMode(), twaDisplayMode))
                        .setEnableUrlBarHiding(true)
                        .setIsCloseButtonEnabled(true)
                        .setCloseButtonIcon(closeButtonIcon)
                        .setRemoteViews(remoteViews)
                        .setSideSheetDecorationType(
                                CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER)
                        .setSideSheetRoundedCornersPosition(
                                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP)
                        .setClickableViewIds(new int[] {101, 102})
                        .setRemoteViewsPendingIntent(remoteViewsPi)
                        .setSecondaryToolbarSwipeUpPendingIntent(swipeUpPi)
                        .setOpenInBrowserState(CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON)
                        .setTranslateLanguage("es")
                        .setAutoTranslateLanguage("fr")
                        .setDefaultOrientation(ScreenOrientation.LANDSCAPE)
                        .setGsaExperimentIds(new int[] {111, 222})
                        .setIsPartialCustomTabFixedHeight(true)
                        .setContentScrollMayResizeTab(true)
                        .setInteractWithBackground(true)
                        .setTabbedCctAllowedByEmbedder(true)
                        .setTabbedCctEnabledByEmbedder(true)
                        .setAuthRedirectScheme("myscheme")
                        .setAuthRedirectHost("auth.example.com")
                        .setAuthRedirectPath("/callback")
                        .setLightColorSchemeParams(
                                new CustomTabColorSchemeParams.Builder()
                                        .setToolbarColor(0xFF112233)
                                        .setSecondaryToolbarColor(0xFF445566)
                                        .setNavigationBarColor(0xFF778899)
                                        .setNavigationBarDividerColor(0xFFAABBCC)
                                        .build())
                        .setDarkColorSchemeParams(
                                new CustomTabColorSchemeParams.Builder()
                                        .setToolbarColor(0xFF000001)
                                        .build())
                        .setLightAuthTabColorSchemeParams(
                                new AuthTabColorSchemeParams.Builder()
                                        .setToolbarColor(0xFF223344)
                                        .setNavigationBarColor(0xFF556677)
                                        .setNavigationBarDividerColor(0xFF8899AA)
                                        .build())
                        .setDarkAuthTabColorSchemeParams(
                                new AuthTabColorSchemeParams.Builder()
                                        .setToolbarColor(0xFF000002)
                                        .build())
                        .setInitialBackgroundColor(0xFF123456)
                        .setCustomButtonParams(List.of(shareButton, bottomBarButton))
                        .setShareState(CustomTabsIntent.SHARE_STATE_ON)
                        .setDefaultShareMenuItem(true)
                        .setOpenInBrowserButtonAllowed(true)
                        .setMenuEntries(
                                List.of(
                                        Pair.create("Menu 1", menuPi),
                                        Pair.create("Menu 2", menuPi2)))
                        .build();

        CustomTabIntentDataHolder restored = roundTripThroughParcel(original);

        assertEquals(sessionHolder, restored.mSessionHolder);
        assertEquals("com.example.client", restored.mClientPackageName);
        assertTrue(restored.mIsTrustedIntent);
        assertNotNull(restored.mAnimationBundle);
        assertEquals("com.example.client", restored.mAnimationBundle.getString("package"));
        assertEquals(10, restored.mAnimationBundle.getInt("enter"));
        assertEquals(20, restored.mAnimationBundle.getInt("exit"));
        assertNotNull(restored.mKeepAliveServiceIntent);
        assertEquals("com.example.KEEP_ALIVE", restored.mKeepAliveServiceIntent.getAction());
        assertEquals(network, restored.mNetwork);
        assertTrue(restored.mIsOpenedByChrome);
        assertEquals(CustomTabsUiType.MEDIA_VIEWER, restored.mUiType);
        assertEquals(TitleVisibility.VISIBLE, restored.mTitleVisibilityState);
        assertEquals(500, restored.mInitialActivityHeight);
        assertEquals(400, restored.mInitialActivityWidth);
        assertEquals(600, restored.mBreakPointDp);
        assertEquals(16, restored.mPartialTabToolbarCornerRadius);
        assertEquals(ActivityType.TRUSTED_WEB_ACTIVITY, restored.mActivityType);
        assertEquals(CustomTabProfileType.EPHEMERAL, restored.mCustomTabMode);
        assertEquals("file:///sdcard/test.mp4", restored.mMediaViewerUrl);
        assertTrue(restored.mEnableEmbeddedMediaExperience);
        assertTrue(restored.mIsFromMediaLauncherActivity);
        assertTrue(restored.mDisableStar);
        assertTrue(restored.mDisableDownload);
        assertEquals(
                List.of("https://a.example.com", "https://b.example.com"),
                restored.mTwaAdditionalOrigins);
        assertTrue(restored.mTwaDisplayMode instanceof ImmersiveMode);
        ImmersiveMode restoredImmersive = (ImmersiveMode) restored.mTwaDisplayMode;
        assertTrue(restoredImmersive.isSticky());
        assertEquals(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                restoredImmersive.layoutInDisplayCutoutMode());
        assertEquals(2, restored.mTwaDisplayOverrideMode.size());
        assertTrue(restored.mTwaDisplayOverrideMode.get(0) instanceof DefaultMode);
        assertTrue(restored.mTwaDisplayOverrideMode.get(1) instanceof ImmersiveMode);
        assertTrue(restored.mEnableUrlBarHiding);
        assertTrue(restored.mIsCloseButtonEnabled);
        assertNotNull(restored.mCloseButtonIcon);
        assertEquals(16, restored.mCloseButtonIcon.getWidth());
        assertNotNull(restored.mRemoteViews);
        assertEquals(
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER,
                restored.mSideSheetDecorationType);
        assertEquals(
                CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_TOP,
                restored.mSideSheetRoundedCornersPosition);
        assertArrayEquals(new int[] {101, 102}, restored.mClickableViewIds);
        assertEquals(remoteViewsPi, restored.mRemoteViewsPendingIntent);
        assertEquals(swipeUpPi, restored.mSecondaryToolbarSwipeUpPendingIntent);
        assertEquals(CustomTabsIntent.OPEN_IN_BROWSER_STATE_ON, restored.mOpenInBrowserState);
        assertEquals("es", restored.mTranslateLanguage);
        assertEquals("fr", restored.mAutoTranslateLanguage);
        assertEquals(ScreenOrientation.LANDSCAPE, restored.mDefaultOrientation);
        assertArrayEquals(new int[] {111, 222}, restored.mGsaExperimentIds);
        assertTrue(restored.mIsPartialCustomTabFixedHeight);
        assertTrue(restored.mContentScrollMayResizeTab);
        assertTrue(restored.mInteractWithBackground);
        assertTrue(restored.mTabbedCctAllowedByEmbedder);
        assertTrue(restored.mTabbedCctEnabledByEmbedder);
        assertEquals("myscheme", restored.mAuthRedirectScheme);
        assertEquals("auth.example.com", restored.mAuthRedirectHost);
        assertEquals("/callback", restored.mAuthRedirectPath);
        assertEquals(Integer.valueOf(0xFF112233), restored.mLightColorSchemeParams.toolbarColor);
        assertEquals(
                Integer.valueOf(0xFF445566),
                restored.mLightColorSchemeParams.secondaryToolbarColor);
        assertEquals(
                Integer.valueOf(0xFF778899), restored.mLightColorSchemeParams.navigationBarColor);
        assertEquals(
                Integer.valueOf(0xFFAABBCC),
                restored.mLightColorSchemeParams.navigationBarDividerColor);
        assertEquals(Integer.valueOf(0xFF000001), restored.mDarkColorSchemeParams.toolbarColor);
        assertNull(restored.mDarkColorSchemeParams.secondaryToolbarColor);
        assertNull(restored.mDarkColorSchemeParams.navigationBarColor);
        assertNull(restored.mDarkColorSchemeParams.navigationBarDividerColor);
        assertEquals(
                Integer.valueOf(0xFF000001),
                restored.getColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_DARK).toolbarColor);
        assertEquals(
                Integer.valueOf(0xFF223344),
                restored.mLightAuthTabColorSchemeParams.getToolbarColor());
        assertEquals(
                Integer.valueOf(0xFF556677),
                restored.mLightAuthTabColorSchemeParams.getNavigationBarColor());
        assertEquals(
                Integer.valueOf(0xFF8899AA),
                restored.mLightAuthTabColorSchemeParams.getNavigationBarDividerColor());
        assertEquals(
                Integer.valueOf(0xFF000002),
                restored.mDarkAuthTabColorSchemeParams.getToolbarColor());
        assertNull(restored.mDarkAuthTabColorSchemeParams.getNavigationBarColor());
        assertNull(restored.mDarkAuthTabColorSchemeParams.getNavigationBarDividerColor());
        assertEquals(
                Integer.valueOf(0xFF000002),
                restored.getAuthTabColorSchemeParams(CustomTabsIntent.COLOR_SCHEME_DARK)
                        .getToolbarColor());
        assertEquals(0xFF123456, restored.mInitialBackgroundColor);
        assertEquals(2, restored.mCustomButtonParams.size());
        CustomButtonParams restoredShare = restored.mCustomButtonParams.get(0);
        assertEquals(CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID, restoredShare.getId());
        assertEquals(shareButton.getDescription(), restoredShare.getDescription());
        assertTrue(restoredShare.showOnToolbar());
        assertEquals(ButtonType.CCT_SHARE_BUTTON, restoredShare.getType());
        assertTrue(restoredShare.getIcon(context) instanceof TintedDrawable);
        assertNotNull(restoredShare.getPendingIntent());

        CustomButtonParams restoredBottom = restored.mCustomButtonParams.get(1);
        assertEquals(8, restoredBottom.getId());
        assertEquals("eight", restoredBottom.getDescription());
        assertFalse(restoredBottom.showOnToolbar());
        assertEquals(ButtonType.OTHER, restoredBottom.getType());
        assertFalse(restoredBottom.getIcon(context) instanceof TintedDrawable);
        assertEquals(swipeUpPi, restoredBottom.getPendingIntent());
        assertEquals(CustomTabsIntent.SHARE_STATE_ON, restored.mShareState);
        assertTrue(restored.mDefaultShareMenuItem);
        assertTrue(restored.mOpenInBrowserButtonAllowed);
        assertEquals(2, restored.mMenuEntries.size());
        assertEquals("Menu 1", restored.mMenuEntries.get(0).first);
        assertEquals(menuPi, restored.mMenuEntries.get(0).second);
        assertEquals("Menu 2", restored.mMenuEntries.get(1).first);
        assertEquals(menuPi2, restored.mMenuEntries.get(1).second);
    }

    @Test
    public void testParcelRoundTrip_customButtonUpdateSurvives() {
        Context context = ContextUtils.getApplicationContext();
        CustomTabIntentDataHolder original =
                new CustomTabIntentDataHolder.Builder()
                        .setCustomButtonParams(
                                List.of(
                                        CustomButtonParamsImpl.createShareButton(
                                                context, Color.BLACK)))
                        .build();
        original.mCustomButtonParams
                .get(0)
                .update(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888), "new");

        CustomTabIntentDataHolder restored = roundTripThroughParcel(original);

        assertEquals(1, restored.mCustomButtonParams.size());
        assertEquals("new", restored.mCustomButtonParams.get(0).getDescription());
    }

    @Test
    public void testParcelRoundTrip_remoteViewsAndSwipeUpUpdatesSurvive() {
        Context context = ContextUtils.getApplicationContext();
        RemoteViews initialViews =
                new RemoteViews(context.getPackageName(), android.R.layout.list_content);
        PendingIntent initialPi =
                PendingIntent.getBroadcast(
                        context, 1, new Intent("initial_rv"), PendingIntent.FLAG_IMMUTABLE);
        PendingIntent initialSwipePi =
                PendingIntent.getBroadcast(
                        context, 2, new Intent("initial_swipe"), PendingIntent.FLAG_IMMUTABLE);
        CustomTabIntentDataHolder holder =
                new CustomTabIntentDataHolder.Builder()
                        .setRemoteViews(initialViews)
                        .setClickableViewIds(new int[] {1})
                        .setRemoteViewsPendingIntent(initialPi)
                        .setSecondaryToolbarSwipeUpPendingIntent(initialSwipePi)
                        .build();

        RemoteViews updatedViews =
                new RemoteViews(context.getPackageName(), android.R.layout.simple_list_item_1);
        PendingIntent updatedPi =
                PendingIntent.getBroadcast(
                        context, 3, new Intent("updated_rv"), PendingIntent.FLAG_IMMUTABLE);
        PendingIntent updatedSwipePi =
                PendingIntent.getBroadcast(
                        context, 4, new Intent("updated_swipe"), PendingIntent.FLAG_IMMUTABLE);
        holder.updateRemoteViews(updatedViews, new int[] {10, 20}, updatedPi);
        holder.setSecondaryToolbarSwipeUpPendingIntent(updatedSwipePi);

        CustomTabIntentDataHolder restored = roundTripThroughParcel(holder);
        assertNotNull(restored.mRemoteViews);
        assertEquals(android.R.layout.simple_list_item_1, restored.mRemoteViews.getLayoutId());
        assertArrayEquals(new int[] {10, 20}, restored.mClickableViewIds);
        assertEquals(updatedPi, restored.mRemoteViewsPendingIntent);
        assertEquals(updatedSwipePi, restored.mSecondaryToolbarSwipeUpPendingIntent);

        restored.updateRemoteViews(null, null, null);
        restored.setSecondaryToolbarSwipeUpPendingIntent(null);
        CustomTabIntentDataHolder restoredCleared = roundTripThroughParcel(restored);
        assertNull(restoredCleared.mRemoteViews);
        assertNull(restoredCleared.mClickableViewIds);
        assertNull(restoredCleared.mRemoteViewsPendingIntent);
        assertNull(restoredCleared.mSecondaryToolbarSwipeUpPendingIntent);
    }

    @Test
    public void testParcelRoundTrip_authTabSession() {
        Context context = ContextUtils.getApplicationContext();
        IBinder binder = new Binder();
        PendingIntent sessionId =
                PendingIntent.getActivity(
                        context, 4, new Intent("auth_session"), PendingIntent.FLAG_IMMUTABLE);
        Intent authIntent = new Intent();
        authIntent.putExtra(AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, true);
        Bundle extras = new Bundle();
        extras.putBinder(CustomTabsIntent.EXTRA_SESSION, binder);
        extras.putParcelable(CustomTabsIntent.EXTRA_SESSION_ID, sessionId);
        authIntent.putExtras(extras);

        SessionHolder authSessionHolder = SessionHolder.getSessionHolderFromIntent(authIntent);
        assertNotNull(authSessionHolder);
        assertTrue(authSessionHolder.isAuthTab());

        CustomTabIntentDataHolder original =
                new CustomTabIntentDataHolder.Builder().setSessionHolder(authSessionHolder).build();
        CustomTabIntentDataHolder restored = roundTripThroughParcel(original);

        assertNotNull(restored.mSessionHolder);
        assertTrue(restored.mSessionHolder.isAuthTab());
        assertEquals(authSessionHolder, restored.mSessionHolder);
    }
}
