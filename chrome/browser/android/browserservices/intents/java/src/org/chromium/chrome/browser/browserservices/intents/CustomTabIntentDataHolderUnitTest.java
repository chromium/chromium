// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Network;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.view.WindowManager;
import android.widget.RemoteViews;

import androidx.browser.auth.AuthTabIntent;
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
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CustomTabProfileType;

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
        assertFalse(restored.mCctTabSwitcherEnabledForChromeExperiment);
        assertFalse(restored.mCctTabSwitcherEnabledForEmbedderExperiment);
        assertNull(restored.mAuthRedirectScheme);
        assertNull(restored.mAuthRedirectHost);
        assertNull(restored.mAuthRedirectPath);
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
                        .setCctTabSwitcherEnabledForChromeExperiment(true)
                        .setCctTabSwitcherEnabledForEmbedderExperiment(true)
                        .setAuthRedirectScheme("myscheme")
                        .setAuthRedirectHost("auth.example.com")
                        .setAuthRedirectPath("/callback")
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
        assertTrue(restored.mCctTabSwitcherEnabledForChromeExperiment);
        assertTrue(restored.mCctTabSwitcherEnabledForEmbedderExperiment);
        assertEquals("myscheme", restored.mAuthRedirectScheme);
        assertEquals("auth.example.com", restored.mAuthRedirectHost);
        assertEquals("/callback", restored.mAuthRedirectPath);
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
