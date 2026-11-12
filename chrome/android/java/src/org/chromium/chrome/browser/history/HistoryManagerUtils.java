// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

/** Utility methods for the browsing history manager. */
@NullMarked
public class HistoryManagerUtils {
    public static final int HISTORY_REQUEST_CODE = 723649;

    /**
     * Opens the browsing history manager. On desktop Android, an off-the-record profile gets the
     * Incognito history disclaimer popup instead, since history isn't recorded in Incognito.
     *
     * @param windowAndroid The {@link WindowAndroid} hosting the {@link HistoryManager}.
     * @param tab The {@link Tab} to display the native page version, or null.
     * @param profile The currently visible {@link Profile}.
     */
    public static void showHistoryManager(
            @Nullable WindowAndroid windowAndroid, @Nullable Tab tab, Profile profile) {
        if (windowAndroid == null) return;
        boolean isDesktopWebUiHistory = HistoryFeatures.isAndroidDesktopWebUiHistoryEnabled();
        if (isDesktopWebUiHistory && profile.isOffTheRecord()) {
            IncognitoHistoryDisclaimerCoordinator.showForWindow(windowAndroid);
            return;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return;
        Context appContext = ContextUtils.getApplicationContext();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity) || isDesktopWebUiHistory) {
            String historyUrl =
                    isDesktopWebUiHistory
                            ? UrlConstants.HISTORY_URL
                            : UrlConstantResolverFactory.getForProfile(profile).getHistoryPageUrl();

            // History shows up as a tab on tablets and desktop Android.
            LoadUrlParams params = new LoadUrlParams(historyUrl);
            ChromeAsyncTabLauncher delegate =
                    new ChromeAsyncTabLauncher(/* incognito= */ profile.isOffTheRecord());
            delegate.launchNewTab(params, TabLaunchType.FROM_CHROME_UI, /* parent= */ tab);
        } else {
            Intent intent = new Intent();
            intent.setClass(appContext, HistoryActivity.class);

            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            ProfileIntentUtils.addProfileToIntent(profile, intent);
            activity.startActivity(intent);
        }
    }

    /**
     * Opens the app specific history manager. For launching history for CCTs, using
     * startActivityForResult to ensure identity sharing.
     *
     * @param activity The {@link Activity} that owns the {@link HistoryManager}.
     * @param profile The currently visible {@link Profile}.
     * @param clientPackageName Package name of the client from which the history activity is
     *     launched.
     */
    // TODO(katzz): Convert to ActivityResult API
    public static void showAppSpecificHistoryManager(
            Activity activity, Profile profile, String clientPackageName) {
        Intent intent = new Intent();
        intent.setClass(activity, HistoryActivity.class);
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        intent.putExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, true);
        intent.putExtra(Intent.EXTRA_PACKAGE_NAME, clientPackageName);
        activity.startActivityForResult(intent, HISTORY_REQUEST_CODE);
    }

    /**
     * Intercepts a load url request for history if it is a history URL and history is disabled in
     * this context (e.g. Incognito desktop Android), showing a popup instead.
     *
     * @param tab The tab loading the url.
     * @param url The url being loaded.
     * @return true if the load url request was intercepted.
     */
    public static boolean maybeInterceptLoadUrl(Tab tab, String url) {
        if (!tab.isIncognitoBranded()
                || !HistoryFeatures.isAndroidDesktopWebUiHistoryEnabled()
                || !isHistoryUrl(url)) {
            return false;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents != null && webContents.getNavigationController().isInitialNavigation()) {
            String ntpUrl = UrlConstantResolverFactory.getForProfile(tab.getProfile()).getNtpUrl();
            tab.loadUrl(new LoadUrlParams(ntpUrl, PageTransition.AUTO_TOPLEVEL));
        }
        IncognitoHistoryDisclaimerCoordinator.showForWindow(tab.getWindowAndroid());
        return true;
    }

    private static boolean isHistoryUrl(String url) {
        Uri uri = Uri.parse(url);
        return UrlUtilities.isChromeScheme(uri.getScheme())
                && UrlConstants.HISTORY_HOST.equalsIgnoreCase(uri.getHost());
    }
}
