// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.feed.FeedServiceDependencyProviderFactory;
import org.chromium.chrome.browser.feed.FeedServiceUtil;

/**
 * Implements the provider factory.
 */
@UsedByReflection("FeedServiceBridge")
public class FeedServiceDependencyProviderFactoryImpl
        implements FeedServiceDependencyProviderFactory {
    private static FeedServiceDependencyProviderFactory sInstance;

    @UsedByReflection("FeedServiceBridge")
    public static FeedServiceDependencyProviderFactory getInstance() {
        if (sInstance == null) {
            sInstance = new FeedServiceDependencyProviderFactoryImpl();
        }
        return sInstance;
    }

    @Override
    public FeedServiceUtil createFeedServiceUtil() {
        return new FeedServiceUtilImpl();
    }
}
