// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

/**
 * Watches for native init to pre-warm the compositor.
 */
public class PaintPreviewTabServiceNativeInitObserver implements NativeInitObserver {
    private ActivityLifecycleDispatcher mActvityLifecycleDispatcher;

    public PaintPreviewTabServiceNativeInitObserver(
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActvityLifecycleDispatcher = activityLifecycleDispatcher;
    }

    @Override
    public void onFinishNativeInitialization() {
        // Warms-up the service and prepares the compositor service.
        PaintPreviewTabServiceFactory.getServiceInstance();
        mActvityLifecycleDispatcher.unregister(this);
    }
}
