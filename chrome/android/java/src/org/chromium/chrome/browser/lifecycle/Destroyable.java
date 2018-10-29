// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * destroy events.
 */
public interface Destroyable extends LifecycleObserver {

    /**
     * Called when activity is being destroyed.
     */
    void destroy();
}
