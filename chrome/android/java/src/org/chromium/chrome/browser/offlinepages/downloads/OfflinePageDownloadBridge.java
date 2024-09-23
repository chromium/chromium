// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.download.home.DownloadActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Serves as an interface between Download Home UI and offline page related items that are to be
 * displayed in the downloads UI.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageDownloadBridge {
    private static OfflinePageDownloadBridge sInstance;
    private static boolean sIsTesting;
    private long mNativeOfflinePageDownloadBridge;

    /**
     * @return An {@link OfflinePageDownloadBridge} instance singleton.  If one
     *         is not available this will create a new one.
     */
    public static OfflinePageDownloadBridge getInstance() {
        if (sInstance == null) {
            sInstance = new OfflinePageDownloadBridge();
        }
        return sInstance;
    }

    private OfflinePageDownloadBridge() {
        mNativeOfflinePageDownloadBridge =
                sIsTesting
                        ? 0L
                        : OfflinePageDownloadBridgeJni.get().init(OfflinePageDownloadBridge.this);
    }

    /** Destroys the native portion of the bridge. */
    public void destroy() {
        if (mNativeOfflinePageDownloadBridge != 0) {
            OfflinePageDownloadBridgeJni.get()
                    .destroy(mNativeOfflinePageDownloadBridge, OfflinePageDownloadBridge.this);
            mNativeOfflinePageDownloadBridge = 0;
        }
    }

    /**
     * 'Opens' the offline page identified by the given URL and offlineId by navigating to the saved
     * local snapshot. No automatic redirection is happening based on the connection status. If the
     * item with specified GUID is not found or can't be opened, nothing happens.
     */
    @CalledByNative
    private static void openItem(
            final @JniType("std::string") String url,
            final long offlineId,
            final int location,
            final boolean isIncognito,
            final boolean openInCct) {
        OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                url,
                offlineId,
                location,
                (params) -> {
                    if (params == null) return;
                    boolean openingFromDownloadsHome =
                            ApplicationStatus.getLastTrackedFocusedActivity()
                                    instanceof DownloadActivity;
                    if (location == LaunchLocation.NET_ERROR_SUGGESTION) {
                        openItemInCurrentTab(offlineId, params);
                    } else if (openInCct && openingFromDownloadsHome) {
                        openItemInCct(offlineId, params, isIncognito);
                    } else {
                        openItemInNewTab(offlineId, params, isIncognito);
                    }
                },
                ProfileManager.getLastUsedRegularProfile());
    }

    /**
     * Opens the offline page identified by the given offlineId and the LoadUrlParams in the current
     * tab. If no tab is current, the page is not opened.
     */
    private static void openItemInCurrentTab(long offlineId, LoadUrlParams params) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) return;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.getUrl()));
        IntentHandler.setIntentExtraHeaders(params.getExtraHeaders(), intent);
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, activity.getApplicationContext().getPackageName());
        intent.setPackage(activity.getApplicationContext().getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    /**
     * Opens the offline page identified by the given offlineId and the LoadUrlParams in a new tab.
     */
    private static void openItemInNewTab(
            long offlineId, LoadUrlParams params, boolean isIncognito) {
        ComponentName componentName = getComponentName();
        AsyncTabCreationParams asyncParams =
                componentName == null
                        ? new AsyncTabCreationParams(params)
                        : new AsyncTabCreationParams(params, componentName);
        final ChromeAsyncTabLauncher chromeAsyncTabLauncher =
                new ChromeAsyncTabLauncher(isIncognito);
        chromeAsyncTabLauncher.launchNewTab(
                asyncParams, TabLaunchType.FROM_CHROME_UI, Tab.INVALID_TAB_ID);
    }

    /** Opens the offline page identified by the given offlineId and the LoadUrlParams in a CCT. */
    private static void openItemInCct(long offlineId, LoadUrlParams params, boolean isIncognito) {
        final Context context;
        if (ApplicationStatus.hasVisibleActivities()) {
            context = ApplicationStatus.getLastTrackedFocusedActivity();
        } else {
            context = ContextUtils.getApplicationContext();
        }

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.addDefaultShareMenuItem();

        CustomTabsIntent customTabIntent = builder.build();
        customTabIntent.intent.setData(Uri.parse(params.getUrl()));

        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.OFFLINE_PAGE);
        // TODO(crbug.com/40731212): Pass isIncognito boolean here after finding a way not to
        // reload the downloaded page for Incognito CCT.
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        IntentUtils.addTrustedIntentExtras(intent);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.setIntentExtraHeaders(params.getExtraHeaders(), intent);

        context.startActivity(intent);
    }

    /**
     * Starts download of the page currently open in the specified Tab.
     * If tab's contents are not yet loaded completely, we'll wait for it
     * to load enough for snapshot to be reasonable. If the Chrome is made
     * background and killed, the background request remains that will
     * eventually load the page in background and obtain its offline
     * snapshot.
     *
     * @param tab a tab contents of which will be saved locally.
     * @param origin the object encapsulating application origin of the request.
     */
    public static void startDownload(Tab tab, OfflinePageOrigin origin) {
        OfflinePageDownloadBridgeJni.get().startDownload(tab, origin.encodeAsJsonString());
    }

    /** Shows a "Downloading ..." toast for the requested items already scheduled for download. */
    @CalledByNative
    public static void showDownloadingToast() {
        DownloadManagerService.getDownloadManagerService()
                .getMessageUiController(/* otrProfileID= */ null)
                .onDownloadStarted();
    }

    /**
     * Method to ensure that the bridge is created for tests without calling the native portion of
     * initialization.
     * @param isTesting flag indicating whether the constructor will initialize native code.
     */
    static void setIsTesting(boolean isTesting) {
        sIsTesting = isTesting;
    }

    private static ComponentName getComponentName() {
        if (!ApplicationStatus.hasVisibleActivities()) return null;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeTabbedActivity) {
            return activity.getComponentName();
        }

        return null;
    }

    @NativeMethods
    interface Natives {
        long init(OfflinePageDownloadBridge caller);

        void destroy(long nativeOfflinePageDownloadBridge, OfflinePageDownloadBridge caller);

        void startDownload(Tab tab, @JniType("std::string") String origin);
    }
}
