// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.init.BrowserParts;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * inflation-related events.
 */
public interface InflationObserver extends LifecycleObserver {

    /**
     * Called immediately before the view hierarchy is inflated.
     * See {@link BrowserParts#preInflationStartup()}.
     */
    void onPreInflationStartup();

    /**
     * Called immediately after the view hierarchy is inflated.
     * See {@link BrowserParts#postInflationStartup()}.
     */
    void onPostInflationStartup();
}
