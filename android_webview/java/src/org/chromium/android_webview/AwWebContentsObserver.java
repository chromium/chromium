// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.AwContents.VisualStateCallback;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Routes notifications from WebContents to AwContentsClient and other listeners. */
@Lifetime.WebView
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
    public void didFinishLoadInPrimaryMainFrame(
            GlobalRenderFrameHostId rfhId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int rfhLifecycleState) {
        if (rfhLifecycleState != LifecycleState.ACTIVE) return;
        String validatedUrl = isKnownValid ? url.getSpec() : url.getPossiblyInvalidSpec();
        if (getClientIfNeedToFireCallback(validatedUrl) != null) {
            mLastDidFinishLoadUrl = validatedUrl;
        }
    }

    @Override
    public void didStartLoading(GURL gurl) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            awContents.releaseDragAndDropPermissions();
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
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            @NetError int errorCode,
            GURL failingGurl,
            @LifecycleState int frameLifecycleState) {
        processFailedLoad(isInPrimaryMainFrame, errorCode, failingGurl);
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

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        String url = navigation.getUrl().getPossiblyInvalidSpec();
        if (navigation.errorCode() != NetError.OK && !navigation.isDownload()) {
            processFailedLoad(true, navigation.errorCode(), navigation.getUrl());
        }

        if (!navigation.hasCommitted()) return;

        mCommittedNavigation = true;

        AwContentsClient client = mAwContentsClient.get();
        if (client != null) {
            // OnPageStarted is not called for in-page navigations, which include fragment
            // navigations and navigation from history.push/replaceState.
            // Error page is handled by AwContentsClientBridge.onReceivedError.
            if (!navigation.isSameDocument()
                    && !navigation.isErrorPage()
                    && AwComputedFlags.pageStartedOnCommitEnabled(
                            navigation.isRendererInitiated())) {
                client.getCallbackHelper().postOnPageStarted(url);
            }

            boolean isReload =
                    (navigation.pageTransition() & PageTransition.CORE_MASK)
                            == PageTransition.RELOAD;
            client.getCallbackHelper().postDoUpdateVisitedHistory(url, isReload);
        }

        // Only invoke the onPageCommitVisible callback when navigating to a different document,
        // but not when navigating to a different fragment within the same document.
        if (!navigation.isSameDocument()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        AwContents awContents = mAwContents.get();
                        if (awContents != null) {
                            awContents.insertVisualStateCallbackIfNotDestroyed(
                                    0,
                                    new VisualStateCallback() {
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

        if (client != null && navigation.isPrimaryMainFrameFragmentNavigation()) {
            // Note fragment navigations do not have a matching onPageStarted.
            client.getCallbackHelper().postOnPageFinished(url);
        }
    }

    public boolean didEverCommitNavigation() {
        return mCommittedNavigation;
    }
}
