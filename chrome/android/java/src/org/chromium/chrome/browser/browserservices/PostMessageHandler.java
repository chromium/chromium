// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PostMessageBackend;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TerminationStatus;
import org.chromium.base.TriState;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.GURLUtils;
import org.chromium.url.GURL;

/**
 * A class that handles postMessage communications with a designated {@link CustomTabsSessionToken}.
 */
@NullMarked
public class PostMessageHandler implements OriginVerificationListener {
    private static final String TAG = "PostMessageHandler";

    // TODO(crbug.com/40257514): This should get moved into androidx.browser.
    private static final String POST_MESSAGE_ORIGIN =
            "androidx.browser.customtabs.POST_MESSAGE_ORIGIN";

    private final MessageCallback mMessageCallback;
    private final PostMessageBackend mPostMessageBackend;
    private @Nullable WebContents mWebContents;
    private @Nullable WebContentsObserver mWebContentsObserver;
    private MessagePort @Nullable [] mChannel;

    /**
     * The origin of the document {@link #mChannel} was created for, captured at channel creation
     * time. Resolving the origin lazily per message would be racy: a message can sit in the UI task
     * queue while a navigation commits, which would attribute it to the wrong document.
     */
    private @Nullable String mChannelOrigin;

    private @Nullable Uri mPostMessageSourceUri;
    private @Nullable Uri mPostMessageTargetUri;

    /**
     * Basic constructor. Everytime the given {@link CustomTabsSessionToken} is associated with a
     * new {@link WebContents},
     * {@link PostMessageHandler#reset(WebContents)} should be called to
     * reset all internal state.
     *
     * @param postMessageBackend The {@link PostMessageBackend} to which updates about the channel
     *                           and posted messages will be sent.
     */
    public PostMessageHandler(PostMessageBackend postMessageBackend) {
        mPostMessageBackend = postMessageBackend;
        mMessageCallback =
                (messagePayload, sentPorts) -> {
                    if (mChannel == null) {
                        Log.e(TAG, "Discarding postMessage as channel is null.");
                        return;
                    }

                    if (mChannel[0].isTransferred()) {
                        Log.e(TAG, "Discarding postMessage as channel has been transferred.");
                        return;
                    }

                    if (mWebContents == null || mWebContents.isDestroyed()) {
                        Log.e(TAG, "Discarding postMessage as web contents has been destroyed.");
                        return;
                    }

                    Bundle bundle = null;
                    if (mChannelOrigin != null && !mChannelOrigin.isEmpty()) {
                        bundle = new Bundle();
                        bundle.putString(POST_MESSAGE_ORIGIN, mChannelOrigin);
                    }
                    assumeNonNull(messagePayload.getAsString());
                    mPostMessageBackend.onPostMessage(messagePayload.getAsString(), bundle);
                };
    }

    /**
     * Resets the internal state of the handler, linking the associated
     * {@link CustomTabsSessionToken} with a new {@link WebContents} and the {@link Tab} that
     * contains it.
     * @param webContents The new {@link WebContents} that the session got associated with. If this
     *                    is null, the handler disconnects and unbinds from service.
     */
    public void reset(final @Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            closeChannelAndForgetWebContents();
            return;
        }
        // Can't reset with the same web contents twice.
        if (webContents.equals(mWebContents)) return;
        if (mWebContents != null) {
            closeChannel();
            if (mWebContentsObserver != null) {
                mWebContentsObserver.observe(null);
                mWebContentsObserver = null;
            }
        }
        mWebContents = webContents;
        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    // A reset can attach to a WebContents that has already committed a document,
                    // for example when a hidden tab is promoted or a tab is swapped in. Starting
                    // from false there would misread the next genuine cross-document navigation as
                    // the initial one and leave the channel alive across the document change.
                    private boolean mNavigatedOnce =
                            !GURL.isEmptyOrInvalid(webContents.getLastCommittedUrl());

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (mNavigatedOnce
                                && navigation.hasCommitted()
                                && !navigation.isSameDocument()) {
                            mPostMessageSourceUri = null;
                            mPostMessageTargetUri = null;
                            closeChannel();
                            return;
                        }
                        if (navigation.hasCommitted()) {
                            mNavigatedOnce = true;
                        }
                    }

                    @Override
                    public void primaryMainFrameRenderProcessGone(
                            @TerminationStatus int terminationStatus) {
                        closeChannelAndForgetWebContents();
                    }

                    @Override
                    public void webContentsDestroyed() {
                        closeChannelAndForgetWebContents();
                    }

                    @Override
                    public void documentLoadedInPrimaryMainFrame(
                            Page page,
                            GlobalRenderFrameHostId rfhId,
                            @LifecycleState int rfhLifecycleState) {
                        if (mChannel != null || mPostMessageSourceUri == null) {
                            return;
                        }
                        initializeWithWebContents(webContents);
                    }
                };
    }

    private boolean isTargetOriginMatchingWebContents(WebContents webContents) {
        String target = mPostMessageTargetUri == null ? "" : mPostMessageTargetUri.toString();
        // An empty or "*" target origin is a wildcard, as understood by
        // WebContents#postMessageToMainFrame and the postMessage specification, so there is
        // nothing to match against.
        if (target.isEmpty() || "*".equals(target)) {
            return true;
        }
        RenderFrameHost mainFrame = webContents.getMainFrame();
        if (mainFrame == null) {
            return false;
        }
        // A sandboxed frame has an opaque security context even though its URL still looks like a
        // regular https:// URL, so the URL comparison below would wrongly match. Reject those
        // outright rather than handing a port to a document that cannot be attributed to an
        // origin.
        org.chromium.url.Origin committedOrigin = mainFrame.getLastCommittedOrigin();
        if (committedOrigin != null && committedOrigin.isOpaque()) {
            return false;
        }
        GURL url = mainFrame.getLastCommittedURL();
        if (url == null || url.isEmpty() || !url.isValid()) {
            return false;
        }
        Origin currentOrigin = Origin.create(url.getSpec());
        Origin targetOrigin = Origin.create(mPostMessageTargetUri);
        if (currentOrigin == null || targetOrigin == null) {
            return false;
        }
        return currentOrigin.equals(targetOrigin);
    }

    private void initializeWithWebContents(final WebContents webContents) {
        if (mPostMessageSourceUri == null || webContents.isDestroyed()) return;
        if (!isTargetOriginMatchingWebContents(webContents)) {
            closeChannel();
            return;
        }
        // Replace any existing channel without tearing down the backend: onDisconnectChannel()
        // unbinds the PostMessageService, which would make the onNotifyMessageChannelReady() call
        // below a no-op and leave the client waiting forever.
        closeChannelPort();
        mChannel = webContents.createMessageChannel();
        mChannel[0].setMessageCallback(mMessageCallback, null);
        mChannelOrigin = getCommittedOrigin(webContents);

        assumeNonNull(mPostMessageSourceUri);
        webContents.postMessageToMainFrame(
                new MessagePayload(""),
                mPostMessageSourceUri.toString(),
                mPostMessageTargetUri != null ? mPostMessageTargetUri.toString() : "",
                new MessagePort[] {mChannel[1]});

        mPostMessageBackend.onNotifyMessageChannelReady(null);
    }

    /**
     * @return The origin of the primary main frame's committed document, in the same format
     *     previously reported to clients via {@link #POST_MESSAGE_ORIGIN}, or null if it cannot be
     *     determined.
     */
    private static @Nullable String getCommittedOrigin(WebContents webContents) {
        RenderFrameHost mainFrame = webContents.getMainFrame();
        if (mainFrame == null) return null;
        GURL url = mainFrame.getLastCommittedURL();
        if (url == null || url.isEmpty() || !url.isValid()) return null;
        return GURLUtils.getOrigin(url.getSpec());
    }

    /**
     * Drops the message channel without notifying the client. {@link MessagePort#close()} throws if
     * the port has already been transferred, so guard against that.
     */
    private void closeChannelPort() {
        if (mChannel == null) return;
        if (!mChannel[0].isClosed() && !mChannel[0].isTransferred()) {
            mChannel[0].close();
        }
        mChannel = null;
        mChannelOrigin = null;
    }

    /**
     * Closes the message channel and notifies the client that it is gone, keeping the {@link
     * WebContents}. The client can re-establish messaging for the new document by calling
     * requestPostMessageChannel() again, which re-verifies the origin and re-enters {@link
     * #initializeWithPostMessageUri}.
     */
    private void closeChannel() {
        if (mChannel == null) return;
        closeChannelPort();
        mPostMessageBackend.onDisconnectChannel(ContextUtils.getApplicationContext());
    }

    /**
     * Closes the message channel and additionally drops the {@link WebContents}, for the cases
     * where it can no longer host a channel at all (destroyed, swapped out, or renderer gone). No
     * channel can be re-established until {@link #reset} supplies a new {@link WebContents}.
     */
    private void closeChannelAndForgetWebContents() {
        closeChannel();
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
        }
        mWebContents = null;
        mPostMessageSourceUri = null;
        mPostMessageTargetUri = null;
    }

    /**
     * Sets the postMessage postMessageUri for this session to the given {@link Uri}.
     *
     * @param postMessageUri The postMessageUri value to be set.
     */
    public void initializeWithPostMessageUri(
            @Nullable Uri postMessageUri, @Nullable Uri targetOrigin) {
        mPostMessageSourceUri = postMessageUri;
        mPostMessageTargetUri = targetOrigin;
        if (mPostMessageSourceUri != null && mWebContents != null && !mWebContents.isDestroyed()) {
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
                () -> {
                    // It is still possible that the page has navigated, or that the port has been
                    // transferred, while this task is in the queue. Both make postMessage() throw,
                    // so fail gracefully.
                    if (mChannel == null || mChannel[0].isClosed() || mChannel[0].isTransferred()) {
                        return;
                    }
                    mChannel[0].postMessage(new MessagePayload(message), null);
                });
        return CustomTabsService.RESULT_SUCCESS;
    }

    @Override
    public void onOriginVerified(
            String packageName, Origin origin, boolean result, @TriState int online) {
        if (!result) return;
        Uri targetOrigin = mPostMessageTargetUri != null ? mPostMessageTargetUri : origin.uri();
        initializeWithPostMessageUri(
                OriginVerifier.getPostMessageUriFromVerifiedOrigin(packageName, origin),
                targetOrigin);
    }

    /**
     * Sets the target origin URI, this should be called before initializing in order for it to
     * work.
     *
     * @param postMessageTargetUri Uri to post the first message to.
     */
    public void setPostMessageTargetUri(@Nullable Uri postMessageTargetUri) {
        mPostMessageTargetUri = postMessageTargetUri;
    }

    public @Nullable Uri getPostMessageTargetUriForTesting() {
        return mPostMessageTargetUri;
    }

    /**
     * @return The PostMessage Uri that has been declared for this handler.
     */
    public @Nullable Uri getPostMessageUriForTesting() {
        return mPostMessageSourceUri;
    }

    public @Nullable WebContents getWebContentsForTesting() {
        return mWebContents;
    }

    public @Nullable WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
