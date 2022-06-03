// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.os.Bundle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onSaveInstanceState events.
 */
public interface SaveInstanceStateObserver extends LifecycleObserver {
    /**
     * Called before activity begins to stop.
     */
    void onSaveInstanceState(Bundle outState);
}
