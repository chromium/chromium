// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import android.content.res.Configuration;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to be notified of
 * configuration changes.
 */
public interface ConfigurationChangedObserver extends LifecycleObserver {
    /**
     * Called when the Activity configuration changes. See
     * {@link android.app.Activity#onConfigurationChanged(Configuration)}.
     */
    void onConfigurationChanged(Configuration newConfig);
}
