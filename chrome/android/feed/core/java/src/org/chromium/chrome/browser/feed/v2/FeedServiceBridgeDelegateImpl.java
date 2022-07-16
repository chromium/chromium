// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.feed.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.hooks.FeedHooks;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.xsurface.ProcessScope;

/**
 * Implementation of {@link FeedServiceBridge.Delegate}.
 */
public class FeedServiceBridgeDelegateImpl implements FeedServiceBridge.Delegate {
    private final FeedHooks mFeedHooks;
    private ProcessScope mXSurfaceProcessScope;

    public FeedServiceBridgeDelegateImpl(FeedHooks feedHooks) {
        mFeedHooks = feedHooks;
    }

    @Override
    public ProcessScope getProcessScope() {
        if (mXSurfaceProcessScope == null) {
            FeedProcessScopeDependencyProvider dependencies =
                    new FeedProcessScopeDependencyProvider(GoogleAPIKeys.GOOGLE_API_KEY,
                            PrivacyPreferencesManagerImpl.getInstance());
            if (mFeedHooks.isEnabled()) {
                mXSurfaceProcessScope = mFeedHooks.createProcessScope(dependencies);
            }
        }
        return mXSurfaceProcessScope;
    }

    @Override
    public void clearAll() {
        FeedSurfaceTracker.getInstance().clearAll();
    }
}
