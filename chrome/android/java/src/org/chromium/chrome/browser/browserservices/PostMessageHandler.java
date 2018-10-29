// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.customtabs.PostMessageBackend;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.browserservices.OriginVerifier.OriginVerificationListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.MessagePort.MessageCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * A class that handles postMessage communications with a designated {@link CustomTabsSessionToken}.
 */
public class PostMessageHandler implements OriginVerificationListener {
    private final MessageCallback mMessageCallback;
    private final PostMessageBackend mPostMessageBackend;
    private WebContents mWebContents;
    private MessagePort[] mChannel;
    private Uri mPostMessageUri;

    /**
     * Basic constructor. Everytime the given {@link CustomTabsSessionToken} is associated with a
     * new {@link WebContents},
     * {@link PostMessageHandler#reset(WebContents)} should be called to
     * reset all internal state.
     * @param postMessageBackend The {@link PostMessageBackend} to which updates about the channel
     *                           and posted messages will be sent.
     */
    public PostMessageHandler(PostMessageBackend postMessageBackend) {
        mPostMessageBackend = postMessageBackend;
        mMessageCallback = new MessageCallback() {
            @Override
            public void onMessage(String message, MessagePort[] sentPorts) {
                mPostMessageBackend.onPostMessage(message, null);
            }
        };
    }

    /**
     * Resets the internal state of the handler, linking the associated
     * {@link CustomTabsSessionToken} with a new {@link WebContents} and the {@link Tab} that
     * contains it.
     * @param webContents The new {@link WebContents} that the session got associated with. If this
     *                    is null, the handler disconnects and unbinds from service.
     */
    public void reset(final WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            disconnectChannel();
            return;
        }
        // Can't reset with the same web contents twice.
        if (webContents.equals(mWebContents)) return;
        mWebContents = webContents;
        if (mPostMessageUri == null) return;
        new WebContentsObserver(webContents) {
            private boolean mNavigatedOnce;

            @Override
            public void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
                    boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
                    boolean isRendererInitiated, boolean isDownload, Integer pageTransition,
                    int errorCode, String errorDescription, int httpStatusCode) {
                if (mNavigatedOnce && hasCommitted && isInMainFrame && !isSameDocument
                        && mChannel != null) {
                    webContents.removeObserver(this);
                    disconnectChannel();
                    return;
                }
                mNavigatedOnce = true;
            }

            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                disconnectChannel();
            }

            @Override
            public void documentLoadedInFrame(long frameId, boolean isMainFrame) {
                if (!isMainFrame || mChannel != null) return;
                initializeWithWebContents(webContents);
            }
        };
    }

    private void initializeWithWebContents(final WebContents webContents) {
        mChannel = webContents.createMessageChannel();
        mChannel[0].setMessageCallback(mMessageCallback, null);

        webContents.postMessageToFrame(
                null, "", mPostMessageUri.toString(), "", new MessagePort[] {mChannel[1]});

        mPostMessageBackend.onNotifyMessageChannelReady(null);
    }

    private void disconnectChannel() {
        if (mChannel == null) return;
        mChannel[0].close();
        mChannel = null;
        mWebContents = null;
        mPostMessageBackend.onDisconnectChannel(ContextUtils.getApplicationContext());
    }

    /**
     * Sets the postMessage postMessageUri for this session to the given {@link Uri}.
     * @param postMessageUri The postMessageUri value to be set.
     */
    public void initializeWithPostMessageUri(Uri postMessageUri) {
        mPostMessageUri = postMessageUri;
        if (mWebContents != null && !mWebContents.isDestroyed()) {
            initializeWithWebContents(mWebContents);
        }
    }

    /**
     * Relay a postMessage request through the current channel assigned to this session.
     * @param message The message to be sent.
     * @return The result of the postMessage request. Returning true means the request was accepted,
     *         not necessarily that the postMessage was successful.
     */
    public int postMessageFromClientApp(final String message) {
        if (mChannel == null || mChannel[0].isClosed()) {
            return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        }
        if (mWebContents == null || mWebContents.isDestroyed()) {
            return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                // It is still possible that the page has navigated while this task is in the queue.
                // If that happens fail gracefully.
                if (mChannel == null || mChannel[0].isClosed()) return;
                mChannel[0].postMessage(message, null);
            }
        });
        return CustomTabsService.RESULT_SUCCESS;
    }

    @Override
    public void onOriginVerified(String packageName, Origin origin, boolean result,
            Boolean online) {
        if (!result) return;
        initializeWithPostMessageUri(
                OriginVerifier.getPostMessageUriFromVerifiedOrigin(packageName, origin));
    }

    /**
     * @return The PostMessage Uri that has been declared for this handler.
     */
    @VisibleForTesting
    public Uri getPostMessageUriForTesting() {
        return mPostMessageUri;
    }
}
