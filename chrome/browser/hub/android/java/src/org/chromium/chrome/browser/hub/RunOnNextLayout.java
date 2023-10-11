// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Interface for {@link View} to provide run on next layout functionality. */
public interface RunOnNextLayout {
    /**
     * Queue a runnable on the next layout if there is one otherwise the runnable will be invoked
     * the next time the UI thread goes idle.
     *
     * @param runnable The {@link Runnable} to run.
     */
    void runOnNextLayout(Runnable runnable);

    /** Run any queued runnables immediately. */
    void runOnNextLayoutRunnables();
}
