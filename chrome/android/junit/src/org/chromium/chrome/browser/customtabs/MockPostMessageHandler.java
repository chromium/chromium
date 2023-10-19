// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;

import android.net.Uri;

import org.mockito.Mockito;
import org.mockito.stubbing.Answer;

import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.components.embedder_support.util.Origin;

/** A mock {@link PostMessageHandler}. */
class MockPostMessageHandler {
    private final PostMessageHandler mPostMessageHandler;
    private Uri mPostMessageUri;
    private Uri mPostMessageTargetUri;

    public MockPostMessageHandler() {
        mPostMessageHandler = Mockito.mock(PostMessageHandler.class);

        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    reset();
                                    return null;
                                })
                .when(mPostMessageHandler)
                .reset(any());

        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    initializeWithPostMessageUri(
                                            invocation.getArgument(0), invocation.getArgument(1));
                                    return null;
                                })
                .when(mPostMessageHandler)
                .initializeWithPostMessageUri(any(), any());

        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    onOriginVerified(
                                            invocation.getArgument(0),
                                            invocation.getArgument(1),
                                            invocation.getArgument(2));
                                    return null;
                                })
                .when(mPostMessageHandler)
                .onOriginVerified(any(), any(), anyBoolean(), any());

        doAnswer(
                        (Answer<Uri>)
                                invocation -> {
                                    return getPostMessageUriForTesting();
                                })
                .when(mPostMessageHandler)
                .getPostMessageUriForTesting();

        doAnswer(
                        (Answer<Uri>)
                                invocation -> {
                                    return getPostMessageTargetUriForTesting();
                                })
                .when(mPostMessageHandler)
                .getPostMessageTargetUriForTesting();

        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    setPostMessageTargetUriForTesting(invocation.getArgument(0));
                                    return null;
                                })
                .when(mPostMessageHandler)
                .setPostMessageTargetUri(any());
    }

    public PostMessageHandler getPostMessageHandler() {
        return mPostMessageHandler;
    }

    public static PostMessageHandler create() {
        return new MockPostMessageHandler().getPostMessageHandler();
    }

    private void reset() {
        mPostMessageUri = null;
        mPostMessageTargetUri = null;
    }

    private void initializeWithPostMessageUri(Uri uri, Uri targetUri) {
        mPostMessageUri = uri;
        mPostMessageTargetUri = targetUri;
    }

    private void onOriginVerified(String packageName, Origin origin, boolean result) {
        if (!result) return;
        initializeWithPostMessageUri(
                ChromeOriginVerifier.getPostMessageUriFromVerifiedOrigin(packageName, origin),
                mPostMessageTargetUri);
    }

    private Uri getPostMessageUriForTesting() {
        return mPostMessageUri;
    }

    private void setPostMessageTargetUriForTesting(Uri targetUri) {
        mPostMessageTargetUri = targetUri;
    }

    private Uri getPostMessageTargetUriForTesting() {
        return mPostMessageTargetUri;
    }
}
