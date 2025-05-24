// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.BooleanSupplier;

/**
 * An implementation of {@link AwContentsIoThreadClient} used by AwContents, not Service Workers.
 *
 * <p>All methods are called on the IO thread.
 */
@Lifetime.WebView
@NullMarked
class AwContentsIoThreadClientImpl extends AwContentsIoThreadClient {
    private final AwSettings mSettings;
    private final AwContentsClient mContentsClient;
    private final ShouldInterceptRequestMediator mShouldInterceptRequestMediator;
    // We could turn this into a delegate if we start needing more things from AwContents.
    private final BooleanSupplier mShouldAcceptCookies;

    public AwContentsIoThreadClientImpl(
            AwSettings settings,
            AwContentsClient contentsClient,
            ShouldInterceptRequestMediator shouldInterceptRequestMediator,
            BooleanSupplier shouldAcceptCookies) {
        mSettings = settings;
        mContentsClient = contentsClient;
        mShouldInterceptRequestMediator = shouldInterceptRequestMediator;
        mShouldAcceptCookies = shouldAcceptCookies;
    }

    @Override
    public int getCacheMode() {
        return mSettings.getCacheMode();
    }

    @Override
    @Nullable
    public ShouldInterceptRequestMediator getShouldInterceptRequestMediator(String url) {
        if (mShouldInterceptRequestMediator.canSkipShouldInterceptRequest(url)) {
            return null;
        }

        return mShouldInterceptRequestMediator;
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

    @Override
    public boolean shouldIncludeCookiesInIntercept() {
        return mSettings.getIncludeCookiesOnIntercept();
    }

    @Override
    public void onLoadResource(String url) {
        mContentsClient.getCallbackHelper().postOnLoadResource(url);
    }
}
