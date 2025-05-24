// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.AwContents.VisualStateCallback;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

/** Routes notifications from WebContents to AwContentsClient and other listeners. */
@Lifetime.WebView
public class AwWebContentsObserver extends WebContentsObserver
        implements Page.PageDeletionListener {
    // TODO(tobiasjs) similarly to WebContentsObserver.mWebContents, mAwContents
    // needs to be a WeakReference, which suggests that there exists a strong
    // reference to an AwWebContentsObserver instance. This is not intentional,
    // and should be found and cleaned up.
    private final WeakReference<AwContents> mAwContents;
    private final WeakReference<AwContentsClient> mAwContentsClient;

    // Maps a NavigationHandle to its associated AwNavigation object. Since the AwNavigation object
    // subclasses AwSupportLibIsomorphic in order to hold onto a reference to the client-side
    // object and we want to keep it stable (so that apps can use the object itself as implicit
    // IDs), we need to keep a mapping. Some important points:
    // - If the app keeps a reference to the app-facing navigation object around, that object will
    //   reference the AwNavigation, which references the NavigationHandle, and so all these
    //   objects will be kept alive: the map will continue to associate the two so that future
    //   callbacks use the same object, and the app can also continue calling the getters even
    //   after //content's native code is no longer keeping the handle around.
    // - If the app doesn't keep a reference to the app-facing navigation object, then the
    //   NavigationHandle will be kept alive by //content for as long as the navigation is in
    //   progress, which will also keep the entry in the hashmap alive, but because the hashmap
    //   value is a weak reference then this will not keep the AwNavigation alive and it might get
    //   GCed in between callbacks; we would have to create a new AwNavigation wrapper if that
    //   happens to call the next callback which is not ideal for performance but doesn't affect
    //   the app-visible behavior of the API much: they didn't keep a reference to the
    //   navigation around the first time and so they can't tell whether the second time is the
    //   same object or not.
    // - The app unfortunately can tell if they store a weak reference to the navigation object,
    //   but there's no need for them to do that here: strongly referencing the object doesn't
    //   leak the WebView or anything.
    WeakHashMap<NavigationHandle, WeakReference<AwNavigation>> mNavigationMap;
    // Similar reason as above, but between Page and AwPage.
    WeakHashMap<Page, WeakReference<AwPage>> mPageMap;

    // Whether this webcontents has ever committed any navigation.
    private boolean mCommittedNavigation;

    // Temporarily stores the URL passed the last time to didFinishLoad callback.
    private String mLastDidFinishLoadUrl;

    public AwWebContentsObserver(
            WebContents webContents, AwContents awContents, AwContentsClient awContentsClient) {
        super(webContents);
        mAwContents = new WeakReference<>(awContents);
        mAwContentsClient = new WeakReference<>(awContentsClient);
        mNavigationMap = new WeakHashMap<>();
        mPageMap = new WeakHashMap<>();
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

    public AwNavigation getAwNavigationFor(NavigationHandle navigation) {
        return getOrUpdateAwNavigationFor(navigation);
    }

    private AwNavigation getOrUpdateAwNavigationFor(NavigationHandle navigation) {
        WeakReference<AwNavigation> awNavigationRef = mNavigationMap.get(navigation);
        if (awNavigationRef != null) {
            AwNavigation awNavigation = awNavigationRef.get();
            if (awNavigation != null) {
                // We're reusing an existing AwNavigation, but the AwPage associated with it might
                // have changed (e.g. if the AwNavigation was created at navigation start it will
                // be constructed with a null page value, but then it commits a page and needs to
                // be updated).
                awNavigation.setPage(getAwPageFor(navigation.getCommittedPage()));
                return awNavigation;
            }
        }
        AwNavigation awNavigation =
                new AwNavigation(navigation, getAwPageFor(navigation.getCommittedPage()));
        mNavigationMap.put(navigation, new WeakReference<>(awNavigation));
        return awNavigation;
    }

    private @Nullable AwPage getAwPageFor(@Nullable Page page) {
        if (page == null) {
            return null;
        }
        WeakReference<AwPage> awPageRef = mPageMap.get(page);
        if (awPageRef != null) {
            AwPage awPage = awPageRef.get();
            if (awPage != null) {
                return awPage;
            }
        }
        AwPage awPage = new AwPage(page);
        // We only keep track of pages that have been the primary page (either the current primary
        // page, or a previously primary but now bfcached / pending deletion page).
        assert !awPage.isPrerendering();
        // Make sure we always track deletion of a non-prerendering page.
        page.setPageDeletionListener(this);
        mPageMap.put(page, new WeakReference<>(awPage));
        return awPage;
    }

    @Override
    public void didFinishLoadInPrimaryMainFrame(
            Page page,
            GlobalRenderFrameHostId rfhId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int rfhLifecycleState) {
        if (rfhLifecycleState != LifecycleState.ACTIVE) return;
        String validatedUrl = isKnownValid ? url.getSpec() : url.getPossiblyInvalidSpec();
        if (getClientIfNeedToFireCallback(validatedUrl) != null) {
            mLastDidFinishLoadUrl = validatedUrl;
        }

        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient client = awContents.getNavigationClient();
            if (client != null) {
                client.onPageLoadEventFired(getAwPageFor(page));
            }
        }
    }

    @Override
    public void documentLoadedInPrimaryMainFrame(
            Page page, GlobalRenderFrameHostId rfhId, @LifecycleState int rfhLifecycleState) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient client = awContents.getNavigationClient();
            if (client != null) {
                client.onPageDOMContentLoadedEventFired(getAwPageFor(page));
            }
        }
    }

    @Override
    public void firstContentfulPaintInPrimaryMainFrame(Page page) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient client = awContents.getNavigationClient();
            if (client != null) {
                client.onFirstContentfulPaint(getAwPageFor(page));
            }
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
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient client = awContents.getNavigationClient();
            if (client != null) {
                client.onNavigationStarted(getOrUpdateAwNavigationFor(navigation));
            }
        }
    }

    @Override
    public void didRedirectNavigation(NavigationHandle navigation) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient client = awContents.getNavigationClient();
            if (client != null && navigation.isInPrimaryMainFrame()) {
                client.onNavigationRedirected(getOrUpdateAwNavigationFor(navigation));
            }
        }
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

        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient navClient = awContents.getNavigationClient();
            if (navClient != null && navigation.isInPrimaryMainFrame()) {
                navClient.onNavigationCompleted(getOrUpdateAwNavigationFor(navigation));
            }
        }

        // Only invoke the onPageCommitVisible callback when navigating to a different document,
        // but not when navigating to a different fragment within the same document.
        if (!navigation.isSameDocument()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        AwContents awContents2 = mAwContents.get();
                        if (awContents2 != null) {
                            awContents2.insertVisualStateCallbackIfNotDestroyed(
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

    @Override
    public void primaryPageChanged(Page page) {
        // The page has become the primary page. If it was a prerendered page before, make sure we
        // no longer consider it as one.
        page.setIsPrerendering(false);
        // Make sure we track the deletion of this new page.
        page.setPageDeletionListener(this);
    }

    @Override
    public void onWillDeletePage(Page page) {
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            AwNavigationClient navClient = awContents.getNavigationClient();
            if (navClient != null && !page.isPrerendering()) {
                navClient.onPageDeleted(getAwPageFor(page));
            }
        }
    }

    public boolean didEverCommitNavigation() {
        return mCommittedNavigation;
    }
}
