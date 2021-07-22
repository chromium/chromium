// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.SystemClock;

import org.chromium.android_webview.AwContents.VisualStateCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/**
 * Routes notifications from WebContents to AwContentsClient and other listeners.
 */
public class AwWebContentsObserver extends WebContentsObserver {
    // TODO(tobiasjs) similarly to WebContentsObserver.mWebContents, mAwContents
    // needs to be a WeakReference, which suggests that there exists a strong
    // reference to an AwWebContentsObserver instance. This is not intentional,
    // and should be found and cleaned up.
    private final WeakReference<AwContents> mAwContents;
    private final WeakReference<AwContentsClient> mAwContentsClient;

    // Whether this webcontents has ever committed any navigation.
    private boolean mCommittedNavigation;

    // Temporarily stores the URL passed the last time to didFinishLoad callback.
    private String mLastDidFinishLoadUrl;

    // The start time for measuring time spent on a page, from commit to the start of the next
    // navigation.
    private long mStartTimeSpentMillis = -1;

    // The scheme for the page we're currently on and measuring time spent for.
    private String mCurrentSchemeForTimeSpent;

    public AwWebContentsObserver(
            WebContents webContents, AwContents awContents, AwContentsClient awContentsClient) {
        super(webContents);
        mAwContents = new WeakReference<>(awContents);
        mAwContentsClient = new WeakReference<>(awContentsClient);
    }

    private AwContentsClient getClientIfNeedToFireCallback(String validatedUrl) {
        AwContentsClient client = mAwContentsClient.get();
        if (client != null) {
            String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
            if (unreachableWebDataUrl == null || !unreachableWebDataUrl.equals(validatedUrl)) {
                return client;
            }
        }
        return null;
    }

    @Override
    public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid,
            boolean isMainFrame, @LifecycleState int rfhLifecycleState) {
        if (rfhLifecycleState != LifecycleState.ACTIVE) return;
        String validatedUrl = isKnownValid ? url.getSpec() : url.getPossiblyInvalidSpec();
        if (isMainFrame && getClientIfNeedToFireCallback(validatedUrl) != null) {
            mLastDidFinishLoadUrl = validatedUrl;
        }
    }

    @Override
    public void didStopLoading(GURL gurl, boolean isKnownValid) {
        String url = isKnownValid ? gurl.getSpec() : gurl.getPossiblyInvalidSpec();
        if (url.length() == 0) url = ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL;
        AwContentsClient client = getClientIfNeedToFireCallback(url);
        if (client != null && url.equals(mLastDidFinishLoadUrl)) {
            client.getCallbackHelper().postOnPageFinished(url);
            mLastDidFinishLoadUrl = null;
        }
    }

    @Override
    public void loadProgressChanged(float progress) {
        AwContentsClient client = mAwContentsClient.get();
        if (client == null) return;
        client.getCallbackHelper().postOnProgressChanged(Math.round(progress * 100));
    }

    @Override
    public void didFailLoad(boolean isMainFrame, @NetError int errorCode, GURL failingGurl,
            @LifecycleState int frameLifecycleState) {
        processFailedLoad(isMainFrame && frameLifecycleState == LifecycleState.ACTIVE, errorCode,
                failingGurl);
    }

    private void processFailedLoad(
            boolean isPrimaryMainFrame, @NetError int errorCode, GURL failingGurl) {
        String failingUrl = failingGurl.getPossiblyInvalidSpec();
        AwContentsClient client = mAwContentsClient.get();
        if (client == null) return;
        String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        boolean isErrorUrl =
                unreachableWebDataUrl != null && unreachableWebDataUrl.equals(failingUrl);
        if (isPrimaryMainFrame && !isErrorUrl) {
            if (errorCode == NetError.ERR_ABORTED) {
                // Need to call onPageFinished for backwards compatibility with the classic webview.
                // See also AwContentsClientBridge.onReceivedError.
                client.getCallbackHelper().postOnPageFinished(failingUrl);
            } else if (errorCode == NetError.ERR_HTTP_RESPONSE_CODE_FAILURE) {
                // This is a HTTP error that results in an error page. We need to call onPageStarted
                // and onPageFinished to have the same behavior with HTTP error navigations that
                // don't result in an error page. See also
                // AwContentsClientBridge.onReceivedHttpError.
                client.getCallbackHelper().postOnPageStarted(failingUrl);
                client.getCallbackHelper().postOnPageFinished(failingUrl);
            }
        }
    }

    @Override
    public void titleWasSet(String title) {
        AwContentsClient client = mAwContentsClient.get();
        if (client == null) return;
        client.updateTitle(title, true);
    }

    /**
     * Converts a scheme to a histogram key used in Android.WebView.PageTimeSpent.{Scheme}. These
     * must be kept in sync.
     */
    private static String pageTimeSpentSchemeToHistogramKey(String scheme) {
        switch (scheme) {
            case UrlConstants.APP_INTENT_SCHEME:
                return "App";
            case UrlConstants.BLOB_SCHEME:
                return "Blob";
            case UrlConstants.CHROME_SCHEME:
                return "Chrome";
            case UrlConstants.CHROME_NATIVE_SCHEME:
                return "ChromeNative";
            case UrlConstants.CONTENT_SCHEME:
                return "Content";
            case UrlConstants.CUSTOM_TAB_SCHEME:
                return "CustomTab";
            case UrlConstants.DATA_SCHEME:
                return "Data";
            case UrlConstants.DEVTOOLS_SCHEME:
                return "Devtools";
            case UrlConstants.DOCUMENT_SCHEME:
                return "Document";
            case UrlConstants.FILE_SCHEME:
                return "File";
            case UrlConstants.FILESYSTEM_SCHEME:
                return "Filesystem";
            case UrlConstants.FTP_SCHEME:
                return "Ftp";
            case UrlConstants.HTTP_SCHEME:
                return "Http";
            case UrlConstants.HTTPS_SCHEME:
                return "Https";
            case UrlConstants.INLINE_SCHEME:
                return "Inline";
            case UrlConstants.INTENT_SCHEME:
                return "Intent";
            case UrlConstants.JAR_SCHEME:
                return "Jar";
            case UrlConstants.JAVASCRIPT_SCHEME:
                return "JavaScript";
            case UrlConstants.SMS_SCHEME:
                return "Sms";
            case UrlConstants.TEL_SCHEME:
                return "Tel";
            default:
                return "Other";
        }
    }

    @Override
    public void didStartNavigation(NavigationHandle navigation) {
        // Time spent on page is measured from navigation commit to the start of the next
        // navigation.
        if (navigation.isInPrimaryMainFrame() && !navigation.isSameDocument()
                && mStartTimeSpentMillis != -1 && mCurrentSchemeForTimeSpent != null) {
            long timeSpentMillis = SystemClock.uptimeMillis() - mStartTimeSpentMillis;
            String key = pageTimeSpentSchemeToHistogramKey(mCurrentSchemeForTimeSpent);
            RecordHistogram.recordLongTimesHistogram100(
                    "Android.WebView.PageTimeSpent." + key, timeSpentMillis);
            mStartTimeSpentMillis = -1;
            mCurrentSchemeForTimeSpent = null;
        }
    }

    @Override
    public void didFinishNavigation(NavigationHandle navigation) {
        String url = navigation.getUrl().getPossiblyInvalidSpec();
        if (navigation.errorCode() != NetError.OK && !navigation.isDownload()) {
            processFailedLoad(
                    navigation.isInPrimaryMainFrame(), navigation.errorCode(), navigation.getUrl());
        }

        // Time spent on page is measured from navigation commit to the start of the next
        // navigation.
        if (navigation.isInPrimaryMainFrame() && !navigation.isSameDocument()) {
            if (navigation.hasCommitted()) {
                mStartTimeSpentMillis = SystemClock.uptimeMillis();
                mCurrentSchemeForTimeSpent = navigation.getUrl().getScheme();
            } else {
                mStartTimeSpentMillis = -1;
                mCurrentSchemeForTimeSpent = null;
            }
        }

        if (!navigation.hasCommitted()) return;

        mCommittedNavigation = true;

        if (!navigation.isInPrimaryMainFrame()) return;

        AwContentsClient client = mAwContentsClient.get();
        if (client != null) {
            // OnPageStarted is not called for in-page navigations, which include fragment
            // navigations and navigation from history.push/replaceState.
            // Error page is handled by AwContentsClientBridge.onReceivedError.
            if (!navigation.isSameDocument() && !navigation.isErrorPage()
                    && AwFeatureList.pageStartedOnCommitEnabled(navigation.isRendererInitiated())) {
                client.getCallbackHelper().postOnPageStarted(url);
            }

            boolean isReload = navigation.pageTransition() != null
                    && ((navigation.pageTransition() & PageTransition.CORE_MASK)
                            == PageTransition.RELOAD);
            client.getCallbackHelper().postDoUpdateVisitedHistory(url, isReload);
        }

        // Only invoke the onPageCommitVisible callback when navigating to a different document,
        // but not when navigating to a different fragment within the same document.
        if (!navigation.isSameDocument()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                AwContents awContents = mAwContents.get();
                if (awContents != null) {
                    awContents.insertVisualStateCallbackIfNotDestroyed(
                            0, new VisualStateCallback() {
                                @Override
                                public void onComplete(long requestId) {
                                    AwContentsClient client1 = mAwContentsClient.get();
                                    if (client1 == null) return;
                                    client1.onPageCommitVisible(url);
                                }
                            });
                }
            });
        }

        if (client != null && navigation.isFragmentNavigation()) {
            // Note fragment navigations do not have a matching onPageStarted.
            client.getCallbackHelper().postOnPageFinished(url);
        }
    }

    public boolean didEverCommitNavigation() {
        return mCommittedNavigation;
    }
}
