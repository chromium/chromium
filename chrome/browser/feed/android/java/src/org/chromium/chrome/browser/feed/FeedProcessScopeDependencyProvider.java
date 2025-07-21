// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.chrome.modules.on_demand.OnDemandModule;

/** Provides logging and context for all surfaces. */
// TODO(b/286003870): Delete the class when all the methods are migrated to
// ProcessScopeDependencyProviderImpl
@JNINamespace("feed::android")
@NullMarked
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private final ImageFetchClient mImageFetchClient;

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

    @Override
    public byte[] getFeedLaunchCuiMetadata() {
        return FeedProcessScopeDependencyProviderJni.get().getFeedLaunchCuiMetadata();
    }

    public static Context createFeedContext(Context context) {
        return BundleUtils.createContextForInflation(context, OnDemandModule.SPLIT_NAME);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        int[] getExperimentIds();

        byte[] getFeedLaunchCuiMetadata();

        @JniType("std::string")
        String getSessionId();

        void processViewAction(byte[] actionData, byte[] loggingParameters);
    }
}
