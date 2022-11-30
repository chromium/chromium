// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

/**
 * Callback to be invoked when a VR exit request was processed.
 */
public interface OnExitVrRequestListener {
    /**
     * Called if the exit VR request was successful and VR was exited.
     */
    void onSucceeded();

    /**
     * Called if the exit request was denied (e.g. user
     * chose to not exit VR).
     */
    void onDenied();
}