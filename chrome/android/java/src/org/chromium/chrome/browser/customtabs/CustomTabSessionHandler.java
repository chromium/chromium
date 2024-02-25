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

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;

import dagger.Lazy;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.NavigationEntry;

import javax.inject.Inject;

/**
 * Implements {@link SessionHandler} for the given instance of Custom Tab activity; registers and
 * unregisters itself in {@link SessionDataHolder}.
 */
@ActivityScope
public class CustomTabSessionHandler implements SessionHandler, StartStopWithNativeObserver {

    private static final String TAG = "CctSessionHandler";

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final Lazy<CustomTabToolbarCoordinator> mToolbarCoordinator;
    private final Lazy<CustomTabBottomBarDelegate> mBottomBarDelegate;
    private final CustomTabIntentHandler mIntentHandler;
    private final CustomTabsConnection mConnection;
    private final SessionDataHolder mSessionDataHolder;
    private final Activity mActivity;

    @Inject
    public CustomTabSessionHandler(
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider tabProvider,
            Lazy<CustomTabToolbarCoordinator> toolbarCoordinator,
            Lazy<CustomTabBottomBarDelegate> bottomBarDelegate,
            CustomTabIntentHandler intentHandler,
            CustomTabsConnection connection,
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SessionDataHolder sessionDataHolder) {
        mIntentDataProvider = intentDataProvider;
        mTabProvider = tabProvider;
        mToolbarCoordinator = toolbarCoordinator;
        mBottomBarDelegate = bottomBarDelegate;
        mIntentHandler = intentHandler;
        mConnection = connection;
        mActivity = activity;
        mSessionDataHolder = sessionDataHolder;
        lifecycleDispatcher.register(this);

        // The active handler will also get set in onStartWithNative, but since native may take some
        // time to initialize, we eagerly set it here to catch any messages the Custom Tabs Client
        // sends our way before that triggers.
        mSessionDataHolder.setActiveHandler(this);
    }

    @Override
    public void onStartWithNative() {
        mSessionDataHolder.setActiveHandler(this);
    }

    @Override
    public void onStopWithNative() {
        mSessionDataHolder.removeActiveHandler(this);
    }

    @Override
    public CustomTabsSessionToken getSession() {
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
            RemoteViews remoteViews, int[] clickableIDs, PendingIntent pendingIntent) {
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
        CustomTabsSessionToken session = mIntentDataProvider.getSession();
        String packageName = mConnection.getClientPackageNameForSession(session);
        if (TextUtils.isEmpty(packageName)) return false;
        Origin origin = Origin.create(referrer);
        if (origin == null) return false;
        return ChromeOriginVerifier.wasPreviouslyVerified(
                packageName, origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
    }
}
