// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.notifications.WebPlatformNotificationMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

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
    public static void launchTab(final int requestId, boolean incognito, GURL url, int disposition,
            String referrerUrl, int referrerPolicy, String extraHeaders,
            ResourceRequestBody postData) {
        WebPlatformNotificationMetrics.getInstance().onNewTabLaunched();

        // Open popup window in custom tab.
        if (disposition == WindowOpenDisposition.NEW_POPUP) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    () -> onWebContentsForRequestAvailable(requestId, null));
            return;
        }

        dispatchLaunch(requestId, incognito, url.getSpec(), referrerUrl, referrerPolicy,
                extraHeaders, postData);
    }

    /** Dispatches the launch event. */
    private static void dispatchLaunch(final int requestId, final boolean incognito,
            final String url, final String referrerUrl, final int referrerPolicy,
            final String extraHeaders, final ResourceRequestBody postData) {
        Context context = ContextUtils.getApplicationContext();

        launchTab(requestId, incognito, url, referrerUrl, referrerPolicy, extraHeaders,
                postData);
    }

    /** Launches WebappActivity or a tab for the |url|. */
    private static void launchTab(int requestId, boolean incognito, String url,
            String referrerUrl, int referrerPolicy, String extraHeaders,
            ResourceRequestBody postData) {
        TabDelegate tabDelegate = new TabDelegate(incognito);

        // Launch into a TrustedWebActivity if one exists for the URL.
        Context appContext = ContextUtils.getApplicationContext();

        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.LINK);
        loadUrlParams.setPostData(postData);
        loadUrlParams.setVerbatimHeaders(extraHeaders);
        loadUrlParams.setReferrer(new Referrer(referrerUrl, referrerPolicy));

        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams,
                requestId);
        tabDelegate.createNewTab(asyncParams, TabLaunchType.FROM_CHROME_UI, Tab.INVALID_TAB_ID);
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
        void onWebContentsForRequestAvailable(int requestId, WebContents webContents);
    }
}
