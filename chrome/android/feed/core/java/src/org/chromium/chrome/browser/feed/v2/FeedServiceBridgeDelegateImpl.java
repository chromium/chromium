// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.xsurface.ProcessScope;

/**
 * Implementation of {@link FeedServiceBridge.Delegate}.
 */
public class FeedServiceBridgeDelegateImpl implements FeedServiceBridge.Delegate {
    private ProcessScope mXSurfaceProcessScope;

    public FeedServiceBridgeDelegateImpl() {}

    @Override
    public ProcessScope getProcessScope() {
        if (mXSurfaceProcessScope == null) {
            mXSurfaceProcessScope = AppHooks.get().getExternalSurfaceProcessScope(
                    new FeedProcessScopeDependencyProvider());
        }
        return mXSurfaceProcessScope;
    }

    @Override
    public void clearAll() {
        FeedSurfaceTracker.getInstance().clearAll();
    }
}
