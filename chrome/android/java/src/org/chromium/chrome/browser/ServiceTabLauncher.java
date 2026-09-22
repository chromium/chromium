// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;
import org.chromium.webapk.lib.client.WebApkIdentityServiceClient;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

import java.util.Collections;
import java.util.List;

/**
 * Tab Launcher to be used to launch new tabs from background Android Services, when it is not known
 * whether an activity is available. It will send an intent to launch the activity.
 *
 * <p>URLs within the scope of a recently launched standalone-capable web app on the Android home
 * screen are launched in the standalone web app frame.
 */
@NullMarked
public class ServiceTabLauncher {
    // Name of the extra containing the Id of a tab launch request id.
    public static final String LAUNCH_REQUEST_ID_EXTRA =
            "org.chromium.chrome.browser.ServiceTabLauncher.LAUNCH_REQUEST_ID";

    /**
     * Launches the browser activity and launches a tab for |url|.
     *
     * @param requestId Id of the request for launching this tab.
     * @param incognito Whether the tab should be launched in incognito mode.
     * @param url The URL which should be launched in a tab.
     * @param disposition The disposition requested by the navigation source.
     * @param referrerUrl URL of the referrer which is opening the page.
     * @param referrerPolicy The referrer policy to consider when applying the referrer.
     * @param extraHeaders Extra headers to apply when requesting the tab's URL.
     * @param postData Post-data to include in the tab URL's request body.
     * @param initiatorOrigin The origin of the initiator of the navigation.
     * @param isRendererInitiated Whether the navigation was initiated from a renderer.
     */
    @CalledByNative
    public static void launchTab(
            int requestId,
            boolean incognito,
            @JniType("GURL") GURL url,
            int disposition,
            @JniType("std::string") String referrerUrl,
            int referrerPolicy,
            @JniType("std::string") String extraHeaders,
            @Nullable ResourceRequestBody postData,
            @JniType("std::optional<url::Origin>") @Nullable Origin initiatorOrigin,
            boolean isRendererInitiated) {
        // Open popup window in custom tab.
        // Note that this is used by PaymentRequestEvent.openWindow().
        // PaymentRequestService.openPaymentHandlerWindow() internally validates that the URL's
        // origin matches the invoked payment app's registered origin before opening the window.
        if (disposition == WindowOpenDisposition.NEW_POPUP) {
            WebContents paymentHandlerWebContent =
                    PaymentRequestService.openPaymentHandlerWindow(url);
            if (paymentHandlerWebContent != null) {
                onWebContentsForRequestAvailable(requestId, paymentHandlerWebContent);
            } else {
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> onWebContentsForRequestAvailable(requestId, null));
            }
            return;
        }

        boolean canLaunchInApp = canLaunchInApp(isRendererInitiated, initiatorOrigin, url);

        dispatchLaunch(
                requestId,
                incognito,
                url.getSpec(),
                referrerUrl,
                referrerPolicy,
                extraHeaders,
                postData,
                initiatorOrigin,
                isRendererInitiated,
                canLaunchInApp);
    }

    /**
     * Determines whether a navigation may be captured into an installed web app (WebAPK, TWA, or
     * standalone window). Renderer-initiated navigations must be strictly same-origin with the
     * target to prevent cross-origin service workers from bypassing navigation initiator tracking
     * and SameSite cookie restrictions.
     */
    @VisibleForTesting
    static boolean canLaunchInApp(
            boolean isRendererInitiated, @Nullable Origin initiatorOrigin, GURL targetUrl) {
        if (!isRendererInitiated) {
            return true;
        }
        if (initiatorOrigin == null || initiatorOrigin.isOpaque()) {
            return false;
        }
        return initiatorOrigin.equals(Origin.create(targetUrl));
    }

    /** Dispatches the launch event. */
    private static void dispatchLaunch(
            int requestId,
            boolean incognito,
            String url,
            String referrerUrl,
            int referrerPolicy,
            String extraHeaders,
            @Nullable ResourceRequestBody postData,
            @Nullable Origin initiatorOrigin,
            boolean isRendererInitiated,
            boolean canLaunchInApp) {
        Context context = ContextUtils.getApplicationContext();

        List<ResolveInfo> resolveInfos;
        String webApkPackageName;
        if (canLaunchInApp) {
            resolveInfos = WebApkValidator.resolveInfosForUrl(context, url);
            webApkPackageName = WebApkValidator.findFirstWebApkPackage(context, resolveInfos);
        } else {
            resolveInfos = Collections.emptyList();
            webApkPackageName = null;
        }

        if (webApkPackageName != null) {
            final List<ResolveInfo> resolveInfosFinal = resolveInfos;
            WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback =
                    (doesBrowserBackWebApk, browserPackageName) -> {
                        if (doesBrowserBackWebApk) {
                            Intent intent =
                                    WebApkNavigationClient.createLaunchWebApkIntent(
                                            webApkPackageName, url, /* forceNavigation= */ true);
                            intent.putExtra(
                                    WebappConstants.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
                            ContextUtils.getApplicationContext().startActivity(intent);
                            return;
                        }
                        launchTabOrWebapp(
                                requestId,
                                incognito,
                                url,
                                referrerUrl,
                                referrerPolicy,
                                extraHeaders,
                                postData,
                                initiatorOrigin,
                                isRendererInitiated,
                                canLaunchInApp,
                                resolveInfosFinal);
                    };
            ChromeWebApkHost.checkChromeBacksWebApkAsync(webApkPackageName, callback);
            return;
        }

        launchTabOrWebapp(
                requestId,
                incognito,
                url,
                referrerUrl,
                referrerPolicy,
                extraHeaders,
                postData,
                initiatorOrigin,
                isRendererInitiated,
                canLaunchInApp,
                resolveInfos);
    }

    /** Launches WebappActivity or a tab for the |url|. */
    private static void launchTabOrWebapp(
            int requestId,
            boolean incognito,
            String url,
            String referrerUrl,
            int referrerPolicy,
            String extraHeaders,
            @Nullable ResourceRequestBody postData,
            @Nullable Origin initiatorOrigin,
            boolean isRendererInitiated,
            boolean canLaunchInApp,
            List<ResolveInfo> resolveInfosForUrl) {
        // Launch WebappActivity if one matches the target URL and was opened recently.
        // Otherwise, open the URL in a tab.
        WebappDataStorage storage =
                canLaunchInApp
                        ? WebappRegistry.getInstance().getWebappDataStorageForUrl(url)
                        : null;
        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(incognito);

        // Launch into a TrustedWebActivity if one exists for the URL.
        Context appContext = ContextUtils.getApplicationContext();
        if (!incognito && canLaunchInApp) {
            Intent twaIntent =
                    TrustedWebActivityClient.getInstance()
                            .createLaunchIntentForTwa(appContext, url, resolveInfosForUrl);

            if (twaIntent != null) {
                appContext.startActivity(twaIntent);
                return;
            }
        }

        // Open a new tab if:
        // - The navigation cannot be launched in an app (e.g. cross-origin renderer initiator).
        // OR
        // - We did not find a WebappDataStorage corresponding to this URL.
        // OR
        // - The WebappDataStorage hasn't been opened recently enough.
        if (!canLaunchInApp || storage == null || !storage.wasUsedRecently()) {
            // If a renderer-initiated navigation somehow lacks an initiator origin, fail closed
            // by attributing it to an opaque origin rather than granting browser-initiated
            // privileges (which would bypass SameSite=Strict cookie restrictions).
            Origin effectiveInitiator = initiatorOrigin;
            if (isRendererInitiated && effectiveInitiator == null) {
                effectiveInitiator = Origin.createOpaqueOrigin();
            }

            LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.LINK);
            if (postData != null) {
                loadUrlParams.setPostData(postData);
            }
            loadUrlParams.setVerbatimHeaders(extraHeaders);
            loadUrlParams.setReferrer(new Referrer(referrerUrl, referrerPolicy));
            loadUrlParams.setInitiatorOrigin(effectiveInitiator);
            loadUrlParams.setIsRendererInitiated(isRendererInitiated);

            AsyncTabCreationParams asyncParams =
                    new AsyncTabCreationParams(loadUrlParams, requestId);
            chromeAsyncTabLauncher.launchNewTab(
                    asyncParams, TabLaunchType.FROM_CHROME_UI, Tab.INVALID_TAB_ID);
        } else {
            // The URL is within the scope of a recently launched standalone-capable web app
            // on the home screen, so open it a standalone web app frame.
            //
            // This currently assumes that the only source is notifications; any future use
            // which adds a different source will need to change this.
            Intent intent = storage.createWebappLaunchIntent();
            assumeNonNull(intent);
            // Replace the web app URL with the URL from the notification. This is within the
            // webapp's scope, so it is valid.
            intent.putExtra(WebappConstants.EXTRA_URL, url);
            intent.putExtra(WebappConstants.EXTRA_SOURCE, ShortcutSource.NOTIFICATION);
            intent.putExtra(WebappConstants.EXTRA_FORCE_NAVIGATION, true);
            chromeAsyncTabLauncher.launchNewStandaloneFrame(intent);
        }
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
    public interface Natives {
        void onWebContentsForRequestAvailable(
                int requestId, @JniType("content::WebContents*") @Nullable WebContents webContents);
    }
}
