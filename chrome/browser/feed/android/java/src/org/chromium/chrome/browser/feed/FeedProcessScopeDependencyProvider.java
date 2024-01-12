// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BundleUtils;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;

/** Provides logging and context for all surfaces. */
// TODO(b/286003870): Delete the class when all the methods are migrated to
// ProcessScopeDependencyProviderImpl
@JNINamespace("feed::android")
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private static final String FEED_SPLIT_NAME = "feedv2";

    private ImageFetchClient mImageFetchClient;

    public FeedProcessScopeDependencyProvider() {
        mImageFetchClient = new FeedImageFetchClient();
    }

    @Override
    public ImageFetchClient getImageFetchClient() {
        return mImageFetchClient;
    }

    @Override
    public long getReliabilityLoggingId() {
        return FeedServiceBridge.getReliabilityLoggingId();
    }

    @Override
    public int[] getExperimentIds() {
        return FeedProcessScopeDependencyProviderJni.get().getExperimentIds();
    }

    public static Context createFeedContext(Context context) {
        return BundleUtils.createContextForInflation(context, FEED_SPLIT_NAME);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        int[] getExperimentIds();

        String getSessionId();

        void processViewAction(byte[] actionData, byte[] loggingParameters);
    }
}
