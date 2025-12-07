// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.widget.RemoteViews;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.NavigationEntry;

import java.util.function.Supplier;

/**
 * Implements {@link SessionHandler} for the given instance of Custom Tab activity; registers and
 * unregisters itself in {@link SessionDataHolder}.
 */
@NullMarked
public class CustomTabSessionHandler implements SessionHandler, StartStopWithNativeObserver {

    private static final String TAG = "CctSessionHandler";

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final Supplier<CustomTabToolbarCoordinator> mToolbarCoordinator;
    private final Supplier<CustomTabBottomBarDelegate> mBottomBarDelegate;
    private final CustomTabIntentHandler mIntentHandler;
    private final Activity mActivity;

    public CustomTabSessionHandler(
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider tabProvider,
            Supplier<CustomTabToolbarCoordinator> toolbarCoordinator,
            Supplier<CustomTabBottomBarDelegate> bottomBarDelegate,
            CustomTabIntentHandler intentHandler,
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mIntentDataProvider = intentDataProvider;
        mTabProvider = tabProvider;
        mToolbarCoordinator = toolbarCoordinator;
        mBottomBarDelegate = bottomBarDelegate;
        mIntentHandler = intentHandler;
        mActivity = activity;
        lifecycleDispatcher.register(this);

        // The active handler will also get set in onStartWithNative, but since native may take some
        // time to initialize, we eagerly set it here to catch any messages the Custom Tabs Client
        // sends our way before that triggers.
        SessionDataHolder.getInstance().setActiveHandler(this);
    }

    @Override
    public void onStartWithNative() {
        SessionDataHolder.getInstance().setActiveHandler(this);
    }

    @Override
    public void onStopWithNative() {
        SessionDataHolder.getInstance().removeActiveHandler(this);
    }

    @Override
    public @Nullable SessionHolder<?> getSession() {
        return mIntentDataProvider.getSession();
    }

    @Override
    public boolean updateCustomButton(int id, Bitmap bitmap, String description) {
        CustomButtonParams params = mIntentDataProvider.getButtonParamsForId(id);
        if (params == null) {
            Log.w(TAG, "Custom toolbar button with ID %d not found", id);
            return false;
        }

        params.update(bitmap, description);
        if (params.showOnToolbar()) {
            return mToolbarCoordinator.get().updateCustomButton(params);
        }
        mBottomBarDelegate.get().updateBottomBarButtons(params);
        return true;
    }

    @Override
    public boolean updateRemoteViews(
            @Nullable RemoteViews remoteViews,
            int @Nullable [] clickableIDs,
            @Nullable PendingIntent pendingIntent) {
        return mBottomBarDelegate.get().updateRemoteViews(remoteViews, clickableIDs, pendingIntent);
    }

    @Override
    public boolean updateSecondaryToolbarSwipeUpPendingIntent(PendingIntent pendingIntent) {
        return mBottomBarDelegate.get().updateSwipeUpPendingIntent(pendingIntent);
    }

    @Override
    public @Nullable Tab getCurrentTab() {
        return mTabProvider.getTab();
    }

    @Override
    public @Nullable String getCurrentUrl() {
        Tab tab = mTabProvider.getTab();
        return tab == null ? null : tab.getUrl().getSpec();
    }

    @Override
    public @Nullable String getPendingUrl() {
        Tab tab = mTabProvider.getTab();
        if (tab == null || tab.getWebContents() == null) return null;

        NavigationEntry entry = tab.getWebContents().getNavigationController().getPendingEntry();
        return entry != null ? entry.getUrl().getSpec() : null;
    }

    @Override
    public int getTaskId() {
        return mActivity.getTaskId();
    }

    @Override
    public Class<? extends Activity> getActivityClass() {
        return mActivity.getClass();
    }

    @Override
    public boolean handleIntent(Intent intent) {
        // This method exists only for legacy reasons, see LaunchIntentDispatcher#
        // clearTopIntentsForCustomTabsEnabled.
        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(
                        intent, mActivity, CustomTabsIntent.COLOR_SCHEME_LIGHT);

        return mIntentHandler.onNewIntent(dataProvider);
    }

    @Override
    public boolean canUseReferrer(Uri referrer) {
        SessionHolder<?> session = mIntentDataProvider.getSession();
        String packageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(session);
        if (TextUtils.isEmpty(packageName)) return false;
        Origin origin = Origin.create(referrer);
        if (origin == null) return false;
        return ChromeOriginVerifier.wasPreviouslyVerified(
                packageName, origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
    }
}
