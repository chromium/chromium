// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;

import java.util.function.BooleanSupplier;

/**
 * An implementation of {@link AwContentsIoThreadClient} used by AwContents, not Service Workers.
 *
 * <p>All methods are called on the IO thread.
 */
@Lifetime.WebView
class AwContentsIoThreadClientImpl extends AwContentsIoThreadClient {
    private final AwSettings mSettings;
    private final AwContentsBackgroundThreadClient mBackgroundThreadClient;
    // We could turn this into a delegate if we start needing more things from AwContents.
    private final BooleanSupplier mShouldAcceptCookies;

     /**
     * Constructor for AwContentsIoThreadClientImpl.
     *
     * @param settings The {@link AwSettings} instance providing configuration options.
     * @param backgroundThreadClient The background thread client for handling specific tasks.
     * @param shouldAcceptCookies A functional interface to determine cookie acceptance policy.
     * @throws IllegalArgumentException If any argument is null.
     */

    public AwContentsIoThreadClientImpl(
            AwSettings settings,
            AwContentsBackgroundThreadClient backgroundThreadClient,
            BooleanSupplier shouldAcceptCookies){
        if (settings == null || backgroundThreadClient == null || shouldAcceptCookies == null) {
            throw new IllegalArgumentException("Arguments cannot be null");
        }
        this.mSettings = settings;
        this.mBackgroundThreadClient = backgroundThreadClient;
        this.mShouldAcceptCookies = shouldAcceptCookies;
    }


    @Override
    public int getCacheMode() {
        return mSettings.getCacheMode();
    }

    @Override
    public AwContentsBackgroundThreadClient getBackgroundThreadClient() {
        return mBackgroundThreadClient;
    }

    @Override
    public boolean shouldBlockContentUrls() {
        return !mSettings.getAllowContentAccess();
    }

    @Override
    public boolean shouldBlockFileUrls() {
        return !mSettings.getAllowFileAccess();
    }

    @Override
    public boolean shouldBlockSpecialFileUrls() {
        return mSettings.getBlockSpecialFileUrls();
    }

    @Override
    public boolean shouldBlockNetworkLoads() {
        return mSettings.getBlockNetworkLoads();
    }

    @Override
    public boolean shouldAcceptCookies() {
        return mShouldAcceptCookies.getAsBoolean();
    }

    @Override
    public boolean shouldAcceptThirdPartyCookies() {
        return mSettings.getAcceptThirdPartyCookies();
    }

    @Override
    public boolean getSafeBrowsingEnabled() {
        return mSettings.getSafeBrowsingEnabled();
    }
}
