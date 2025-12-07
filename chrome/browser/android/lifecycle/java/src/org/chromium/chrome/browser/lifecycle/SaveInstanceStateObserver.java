// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onSaveInstanceState events.
 */
@NullMarked
public interface SaveInstanceStateObserver extends LifecycleObserver {
    /** Called before activity begins to stop. */
    void onSaveInstanceState(Bundle outState);
}
