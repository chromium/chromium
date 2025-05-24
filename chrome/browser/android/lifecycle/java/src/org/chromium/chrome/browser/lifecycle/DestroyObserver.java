// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import org.chromium.build.annotations.NullMarked;

/**
 * Implement this interface and register in {@link
 * org.chromium.chrome.browser.init.ActivityLifecycleDispatcher} to receive destroy events.
 */
@NullMarked
public interface DestroyObserver extends LifecycleObserver {
    /** Called when activity is being destroyed. */
    void onDestroy();
}
