// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PostMessageBackend;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TerminationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.MessagePort.MessageCallback;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.GURLUtils;
import org.chromium.url.GURL;

/**
 * A class that handles postMessage communications with a designated {@link CustomTabsSessionToken}.
 */
public class PostMessageHandler implements OriginVerificationListener {
    private static final String TAG = "PostMessageHandler";

    // TODO(crbug.com/40257514): This should get moved into androidx.browser.
    private static final String POST_MESSAGE_ORIGIN =
            "androidx.browser.customtabs.POST_MESSAGE_ORIGIN";

    private final MessageCallback mMessageCallback;
    private final PostMessageBackend mPostMessageBackend;
    private WebContents mWebContents;
    private MessagePort[] mChannel;
    private Uri mPostMessageSourceUri;
    private Uri mPostMessageTargetUri;

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
        mMessageCallback =
                (messagePayload, sentPorts) -> {
                    if (mChannel[0].isTransferred()) {
                        Log.e(TAG, "Discarding postMessage as channel has been transferred.");
                        return;
                    }

                    if (mWebContents == null || mWebContents.isDestroyed()) {
                        Log.e(TAG, "Discarding postMessage as web contents has been destroyed.");
                        return;
                    }

                    Bundle bundle = null;
                    GURL url = mWebContents.getMainFrame().getLastCommittedURL();
                    if (url != null) {
                        String origin = GURLUtils.getOrigin(url.getSpec());
                        bundle = new Bundle();
                        bundle.putString(POST_MESSAGE_ORIGIN, origin);
                    }
                    mPostMessageBackend.onPostMessage(messagePayload.getAsString(), bundle);
                    RecordHistogram.recordBooleanHistogram(
                            "CustomTabs.PostMessage.OnMessage", true);
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
        if (mPostMessageSourceUri == null) return;
        new WebContentsObserver(webContents) {
            private boolean mNavigatedOnce;

            @Override
            public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                if (mNavigatedOnce
                        && navigation.hasCommitted()
                        && !navigation.isSameDocument()
                        && mChannel != null) {
                    webContents.removeObserver(this);
                    disconnectChannel();
                    return;
                }
                mNavigatedOnce = true;
            }

            @Override
            public void primaryMainFrameRenderProcessGone(
                    @TerminationStatus int terminationStatus) {
                disconnectChannel();
            }

            @Override
            public void documentLoadedInPrimaryMainFrame(
                    GlobalRenderFrameHostId rfhId, @LifecycleState int rfhLifecycleState) {
                if (mChannel != null) {
                    return;
                }
                initializeWithWebContents(webContents);
            }
        };
    }

    private void initializeWithWebContents(final WebContents webContents) {
        mChannel = webContents.createMessageChannel();
        mChannel[0].setMessageCallback(mMessageCallback, null);

        webContents.postMessageToMainFrame(
                new MessagePayload(""),
                mPostMessageSourceUri.toString(),
                mPostMessageTargetUri != null ? mPostMessageTargetUri.toString() : "",
                new MessagePort[] {mChannel[1]});

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
    public void initializeWithPostMessageUri(Uri postMessageUri, Uri targetOrigin) {
        mPostMessageSourceUri = postMessageUri;
        mPostMessageTargetUri = targetOrigin;
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
        if (mChannel[0].isTransferred()) {
            Log.e(TAG, "Not sending postMessage as channel has been transferred.");
            return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        }
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        // It is still possible that the page has navigated while this task is in
                        // the queue.
                        // If that happens fail gracefully.
                        if (mChannel == null || mChannel[0].isClosed()) return;
                        mChannel[0].postMessage(new MessagePayload(message), null);
                    }
                });
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.PostMessage.PostMessageFromClientApp", true);
        return CustomTabsService.RESULT_SUCCESS;
    }

    @Override
    public void onOriginVerified(
            String packageName, Origin origin, boolean result, Boolean online) {
        if (!result) return;
        initializeWithPostMessageUri(
                OriginVerifier.getPostMessageUriFromVerifiedOrigin(packageName, origin),
                mPostMessageTargetUri);
    }

    /**
     * Sets the target origin URI, this should be called before initializing in order for it to
     * work.
     *
     * @param postMessageTargetUri Uri to post the first message to.
     */
    public void setPostMessageTargetUri(Uri postMessageTargetUri) {
        mPostMessageTargetUri = postMessageTargetUri;
    }

    public Uri getPostMessageTargetUriForTesting() {
        return mPostMessageTargetUri;
    }

    /**
     * @return The PostMessage Uri that has been declared for this handler.
     */
    public Uri getPostMessageUriForTesting() {
        return mPostMessageSourceUri;
    }
}
