// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.BrowserServicesMetrics;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.payments.PaymentRequestImpl;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.webapk.lib.client.WebApkIdentityServiceClient;
import org.chromium.webapk.lib.client.WebApkNavigationClient;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.List;

/**
 * Tab Launcher to be used to launch new tabs from background Android Services,
 * when it is not known whether an activity is available. It will send an intent to launch the
 * activity.
 *
 * URLs within the scope of a recently launched standalone-capable web app on the Android home
 * screen are launched in the standalone web app frame.
 */
public class ServiceTabLauncher {
    // Name of the extra containing the Id of a tab launch request id.
    public static final String LAUNCH_REQUEST_ID_EXTRA =
            "org.chromium.chrome.browser.ServiceTabLauncher.LAUNCH_REQUEST_ID";

    /**
     * Launches the browser activity and launches a tab for |url|.
     *
     * @param requestId      Id of the request for launching this tab.
     * @param incognito      Whether the tab should be launched in incognito mode.
     * @param url            The URL which should be launched in a tab.
     * @param disposition    The disposition requested by the navigation source.
     * @param referrerUrl    URL of the referrer which is opening the page.
     * @param referrerPolicy The referrer policy to consider when applying the referrer.
     * @param extraHeaders   Extra headers to apply when requesting the tab's URL.
     * @param postData       Post-data to include in the tab URL's request body.
     */
    @CalledByNative
    public static void launchTab(final int requestId, boolean incognito, String url,
            int disposition, String referrerUrl, int referrerPolicy, String extraHeaders,
            ResourceRequestBody postData) {
        // Open popup window in custom tab.
        // Note that this is used by PaymentRequestEvent.openWindow().
        if (disposition == WindowOpenDisposition.NEW_POPUP) {
            boolean success = false;
            try {
                success = PaymentHandlerCoordinator.isEnabled()
                        ? PaymentRequestImpl.openPaymentHandlerWindow(new URI(url))
                        : createPopupCustomTab(requestId, url, incognito);
            } catch (URISyntaxException e) { /* Intentionally leave blank, so success is false. */
            }
            if (!success) {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                        () -> onWebContentsForRequestAvailable(requestId, null));
            }
            return;
        }

        dispatchLaunch(
                requestId, incognito, url, referrerUrl, referrerPolicy, extraHeaders, postData);
    }

    /** Dispatches the launch event. */
    private static void dispatchLaunch(final int requestId, final boolean incognito,
            final String url, final String referrerUrl, final int referrerPolicy,
            final String extraHeaders, final ResourceRequestBody postData) {
        Context context = ContextUtils.getApplicationContext();

        List<ResolveInfo> resolveInfos;
        try (BrowserServicesMetrics.TimingMetric t =
                     BrowserServicesMetrics.getServiceTabResolveInfoTimingContext()) {
            resolveInfos = WebApkValidator.resolveInfosForUrl(context, url);
        }
        String webApkPackageName = WebApkValidator.findFirstWebApkPackage(context, resolveInfos);

        if (webApkPackageName != null) {
            final List<ResolveInfo> resolveInfosFinal = resolveInfos;
            WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback =
                    (doesBrowserBackWebApk, browserPackageName) -> {
                if (doesBrowserBackWebApk) {
                    Intent intent = WebApkNavigationClient.createLaunchWebApkIntent(
                            webApkPackageName, url, true /* forceNavigation */);
                    intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
                    ContextUtils.getApplicationContext().startActivity(intent);
                    return;
                }
                launchTabOrWebapp(requestId, incognito, url, referrerUrl, referrerPolicy,
                        extraHeaders, postData, resolveInfosFinal);
            };
            ChromeWebApkHost.checkChromeBacksWebApkAsync(webApkPackageName, callback);
            return;
        }

        launchTabOrWebapp(requestId, incognito, url, referrerUrl, referrerPolicy, extraHeaders,
                postData, resolveInfos);
    }

    /** Launches WebappActivity or a tab for the |url|. */
    private static void launchTabOrWebapp(int requestId, boolean incognito, String url,
            String referrerUrl, int referrerPolicy, String extraHeaders,
            ResourceRequestBody postData, List<ResolveInfo> resolveInfosForUrl) {
        // Launch WebappActivity if one matches the target URL and was opened recently.
        // Otherwise, open the URL in a tab.
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorageForUrl(url);
        TabDelegate tabDelegate = new TabDelegate(incognito);

        // Launch into a TrustedWebActivity if one exists for the URL.
        Context appContext = ContextUtils.getApplicationContext();
        if (!incognito) {
            Intent twaIntent = TrustedWebActivityClient
                    .createLaunchIntentForTwa(appContext, url, resolveInfosForUrl);

            if (twaIntent != null) {
                appContext.startActivity(twaIntent);
                return;
            }
        }

        // Open a new tab if:
        // - We did not find a WebappDataStorage corresponding to this URL.
        // OR
        // - The WebappDataStorage hasn't been opened recently enough.
        if (storage == null || !storage.wasUsedRecently()) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.LINK);
            loadUrlParams.setPostData(postData);
            loadUrlParams.setVerbatimHeaders(extraHeaders);
            loadUrlParams.setReferrer(new Referrer(referrerUrl, referrerPolicy));

            AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams,
                    requestId);
            tabDelegate.createNewTab(asyncParams, TabLaunchType.FROM_CHROME_UI, Tab.INVALID_TAB_ID);
        } else {
            // The URL is within the scope of a recently launched standalone-capable web app
            // on the home screen, so open it a standalone web app frame.
            //
            // This currently assumes that the only source is notifications; any future use
            // which adds a different source will need to change this.
            Intent intent = storage.createWebappLaunchIntent();
            // Replace the web app URL with the URL from the notification. This is within the
            // webapp's scope, so it is valid.
            intent.putExtra(ShortcutHelper.EXTRA_URL, url);
            intent.putExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
            intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, true);
            tabDelegate.createNewStandaloneFrame(intent);
        }
    }

    /**
     * Creates a popup custom tab to open the url. The popup tab is animated in from bottom to top
     * and out from top to bottom.
     * Note that this is used by PaymentRequestEvent.openWindow().
     *
     * @param requestId   The tab launch request ID from the {@link ServiceTabLauncher}.
     * @param url         The url to open in the new tab.
     */
    private static boolean createPopupCustomTab(int requestId, String url, boolean incognito) {
        // Do not open the popup custom tab if the chrome activity is not in the forground.
        Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(lastTrackedActivity instanceof ChromeActivity)) return false;

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setStartAnimations(lastTrackedActivity, R.anim.slide_in_up, 0);
        builder.setExitAnimations(lastTrackedActivity, 0, R.anim.slide_out_down);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        customTabsIntent.intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA, requestId);
        customTabsIntent.intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, incognito);
        customTabsIntent.intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());

        // Customize items on menu as payment request UI to show 'Find in page', 'Forward arrow',
        // 'Info' and 'Refresh' only.
        CustomTabIntentDataProvider.addPaymentRequestUIExtras(customTabsIntent.intent);

        customTabsIntent.launchUrl(lastTrackedActivity, Uri.parse(url));

        return true;
    }

    /**
     * To be called by the activity when the WebContents for |requestId| has been created, or has
     * been recycled from previous use. The |webContents| must not yet have started provisional
     * load for the main frame.
     * The |webContents| could be null if the request is failed.
     *
     * @param requestId Id of the tab launching request which has been fulfilled.
     * @param webContents The WebContents instance associated with this request.
     */
    public static void onWebContentsForRequestAvailable(
            int requestId, @Nullable WebContents webContents) {
        ServiceTabLauncherJni.get().onWebContentsForRequestAvailable(requestId, webContents);
    }

    @NativeMethods
    interface Natives {
        void onWebContentsForRequestAvailable(int requestId, WebContents webContents);
    }
}
