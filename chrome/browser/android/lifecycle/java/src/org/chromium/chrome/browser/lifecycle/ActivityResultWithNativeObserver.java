// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.content.Intent;

import org.chromium.build.annotations.NullMarked;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * activity result methods.
 */
@NullMarked
public interface ActivityResultWithNativeObserver extends LifecycleObserver {
    /**
     * Called when {@link
     * org.chromium.chrome.browser.init.AsyncInitializationActivity#onActivityResult(int, int,
     * Intent)} is called.
     */
    void onActivityResultWithNative(int requestCode, int resultCode, Intent data);
}
