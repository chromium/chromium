// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

/**
 * Empty implementation of the browser parts for easy extension.
 */
public class EmptyBrowserParts implements BrowserParts {

    @Override
    public void preInflationStartup() {
    }

    @Override
    public void setContentViewAndLoadLibrary(Runnable onInflationCompleteCallback) {
        onInflationCompleteCallback.run();
    }

    @Override
    public void postInflationStartup() {
    }

    @Override
    public void maybePreconnect() {
    }

    @Override
    public void initializeCompositor() {
    }

    @Override
    public void initializeState() {
    }

    @Override
    public void finishNativeInitialization() {
    }

    @Override
    public void onStartupFailure(Exception failureCause) {}

    @Override
    public boolean isActivityFinishingOrDestroyed() {
        return false;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }
}
